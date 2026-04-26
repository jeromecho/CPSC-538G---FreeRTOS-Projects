#include "ProjectConfig.h"

#if USE_EDF

#include "admission_control.h"

#include "edf_scheduler.h"
#include "helpers.h"
#include "tracer.h"

#include "scheduler_internal.h"

#if USE_SRP
#include "srp.h"
#endif // USE_SRP

#include <math.h>
#include <stdbool.h>
#include <string.h>

; // =================
; // === CONSTANTS ===
; // =================

#define EPSILON 1e-9

void admission_control_handle_failure(const uint32_t uid) {
  TRACE_record(EVENT_ADMISSION_FAIL(uid), TRACE_TASK_PERIODIC, NULL, false);
  TRACE_disable();
  xTaskNotifyGive(monitor_task_handle);
}

; // =============================================
; // === LIGHTWEIGHT PERIODIC TASK ABSTRACTION ===
; // =============================================

/// @brief Lightweight representation of a periodic task's scheduling parameters.
typedef struct {
  TickType_t C; // Completion time (WCET)
  TickType_t T; // Period
  TickType_t D; // Relative deadline
} PeriodicTask_t;

typedef struct {
  PeriodicTask_t *tasks;
  size_t          count;
  size_t          capacity;
} PeriodicTaskSet_t;

static PeriodicTask_t periodic_task_from_tmb(const TMB_t *task) {
  configASSERT(task != NULL);
  configASSERT(task->type == TASK_PERIODIC);
  return (PeriodicTask_t){
    .C = task->completion_time,
    .T = task->periodic.period,
    .D = task->periodic.relative_deadline,
  };
}

static PeriodicTaskSet_t initialize_task_set(PeriodicTask_t *tasks, const size_t capacity) {
  PeriodicTask_t    empty_task = {0};
  PeriodicTaskSet_t task_set   = {
      .tasks    = tasks,
      .count    = 0,
      .capacity = capacity,
  };
  return task_set;
}

static BaseType_t add_task_to_task_set(const PeriodicTask_t *const new_task, PeriodicTaskSet_t *const task_set) {
  if (task_set == NULL || task_set->tasks == NULL || task_set->count >= task_set->capacity) {
    return pdFAIL;
  }

  task_set->tasks[task_set->count++] = *new_task;
  return pdPASS;
}

static PeriodicTaskSet_t build_periodic_task_set_from_view_set( //
  const TMBViewSet_t *const view_set,
  PeriodicTask_t *const     task_buffer,
  const size_t              buffer_capacity
) {
  if (task_buffer == NULL || view_set == NULL || view_set->view == NULL) {
    return initialize_task_set(NULL, 0);
  }

  PeriodicTaskSet_t out_task_set = initialize_task_set(task_buffer, buffer_capacity);

  for (size_t i = 0; i < view_set->count; i++) {
    const TMB_t *task = view_set->view[i];
    if (task == NULL || task->handle == NULL) {
      continue;
    }
    if (out_task_set.count >= out_task_set.capacity) {
      break;
    }

    PeriodicTask_t periodic_task = periodic_task_from_tmb(task);
    (void)add_task_to_task_set(&periodic_task, &out_task_set);
  }

  return out_task_set;
}


; // ===================================
; // === LOCAL FUNCTION DECLARATIONS ===
; // ===================================


static double     dbf( //
  const TickType_t               L,
  const PeriodicTask_t *const    new_task,
  const PeriodicTaskSet_t *const task_set
);
static double     calculate_l_star( //
  const PeriodicTask_t *const    new_task,
  const double                   U,
  const PeriodicTaskSet_t *const task_set
);
static TickType_t calculate_d_max( //
  const PeriodicTask_t *const    new_task,
  const PeriodicTaskSet_t *const task_set
);

#if TEST_SUITE == TEST_SUITE_EDF || TEST_SUITE == TEST_SUITE_CBS || TEST_SUITE == TEST_SUITE_PARTITIONED_MP
static bool check_deadlines_edf( //
  const PeriodicTask_t *const    new_task,
  const TickType_t               upper,
  const PeriodicTaskSet_t *const task_set
);
#endif // TEST_SUITE == TEST_SUITE_EDF || TEST_SUITE == TEST_SUITE_CBS || TEST_SUITE == TEST_SUITE_PARTITIONED_MP

#if USE_SRP
static TickType_t calculate_blocking_time(
  const unsigned int target_preemption_level,
  const unsigned int simulated_ceilings[N_RESOURCES],
  const unsigned int new_task_preemption_level,
  const TickType_t   new_task_resource_holds[N_RESOURCES]
);
static TickType_t calculate_B_L(
  const TickType_t   L,
  const unsigned int simulated_ceilings[N_RESOURCES],
  const unsigned int new_preemption_level,
  const TickType_t   new_resource_holds[N_RESOURCES],
  const TickType_t   new_relative_deadline
);
static bool check_deadlines_srp(
  const PeriodicTask_t *const    new_task,
  const TickType_t               upper,
  const unsigned int             preemption_level,
  const TickType_t               resource_hold_times[N_RESOURCES],
  const unsigned int             simulated_ceilings[N_RESOURCES],
  const PeriodicTaskSet_t *const task_set
);
#endif // USE_SRP


; // ==========================================
; // === COMMON HELPER FUNCTIONS (EDF & SRP) ==
; // ==========================================

/// @brief Demand bound function (dbf) - assumes task set is synchronized
static double dbf( //
  const TickType_t               L,
  const PeriodicTask_t *const    new_task,
  const PeriodicTaskSet_t *const task_set
) {
  double       demand = 0.0;
  const double C_new  = (double)new_task->C;
  const double T_new  = (double)new_task->T;
  const double D_new  = (double)new_task->D;

  if (task_set == NULL || task_set->tasks == NULL) {
    return (L >= D_new) ? (floor((double)(L + T_new - D_new) / T_new) * C_new) : 0.0;
  }

  // Existing tasks
  for (size_t i = 0; i < task_set->count; i++) {
    const PeriodicTask_t *task = &task_set->tasks[i];
    const TickType_t      Ci   = task->C;
    const TickType_t      Ti   = task->T;
    const TickType_t      Di   = task->D;

    // Only calculate if L is past the first deadline to avoid negative floor results
    if (L >= Di) {
      demand += (floor((double)(L + Ti - Di) / Ti)) * Ci;
    }
  }

  // New task
  if (L >= D_new) {
    demand += (floor((double)(L + T_new - D_new) / T_new)) * C_new;
  }
  return demand;
}

/// @brief This calculation is taken from Theorem 4.6 in the book Hard Real-Time Computing Systems, Fourth Edition.
static double calculate_l_star( //
  const PeriodicTask_t *const    new_task,
  const double                   utilization,
  const PeriodicTaskSet_t *const task_set
) {
  double           numerator = 0.0;
  const TickType_t C_new     = new_task->C;
  const TickType_t T_new     = new_task->T;
  const TickType_t D_new     = new_task->D;

  if (task_set != NULL && task_set->tasks != NULL) {
    for (size_t i = 0; i < task_set->count; i++) {
      const PeriodicTask_t *task = &task_set->tasks[i];
      const double          Ci   = (double)task->C;
      const double          Ti   = (double)task->T;
      const double          Di   = (double)task->D;
      const double          Ui   = Ci / Ti;

      numerator += (Ti - Di) * Ui;
    }
  }

  const double new_utilization = (double)C_new / T_new;
  numerator += (T_new - D_new) * new_utilization;

  return numerator / (1.0 - utilization);
}

/// @brief Finds the largest relative deadline between the one provided, and the already existing periodic tasks.
static TickType_t calculate_d_max( //
  const PeriodicTask_t *const    new_task,
  const PeriodicTaskSet_t *const task_set
) {
  TickType_t D_max = new_task->D;
  if (task_set == NULL || task_set->tasks == NULL) {
    return D_max;
  }

  for (size_t i = 0; i < task_set->count; i++) {
    if (task_set->tasks[i].D > D_max) {
      D_max = task_set->tasks[i].D;
    }
  }
  return D_max;
}

; // ==================================
; // === PURE EDF ADMISSION CONTROL ===
; // ==================================

#if TEST_SUITE == TEST_SUITE_EDF || TEST_SUITE == TEST_SUITE_CBS || TEST_SUITE == TEST_SUITE_PARTITIONED_MP

/// @brief checks if demand bound functions evaluates to leq L at points of interest
static bool check_deadlines_edf( //
  const PeriodicTask_t *const    new_task,
  const TickType_t               upper,
  const PeriodicTaskSet_t *const task_set
) {
  if (new_task == NULL) {
    return false;
  }

  if (task_set == NULL || task_set->tasks == NULL) {
    return true;
  }

  for (size_t i = 0; i < task_set->count; i++) {
    const TickType_t Ti = task_set->tasks[i].T;
    const TickType_t Di = task_set->tasks[i].D;

    for (TickType_t k = 0;; k++) {
      const TickType_t t = k * Ti + Di;
      if (t > upper)
        break;

      if (dbf(t, new_task, task_set) > (double)t) {
        return false;
      }
    }
  }

  for (TickType_t k = 0;; k++) {
    const TickType_t t = k * new_task->T + new_task->D;
    if (t > upper)
      break;

    if (dbf(t, new_task, task_set) > (double)t) {
      return false;
    }
  }

  return true;
}

bool EDF_can_admit_periodic_task_for_task_set( //
  const TickType_t    C_new,
  const TickType_t    T_new,
  const TickType_t    D_new,
  const TMBViewSet_t *task_view_set
) {
  if (task_view_set->count >= MAXIMUM_PERIODIC_TASKS) {
    return false;
  }

  PeriodicTask_t    existing_tasks[MAXIMUM_PERIODIC_TASKS];
  PeriodicTaskSet_t existing = build_periodic_task_set_from_view_set( //
    task_view_set,
    existing_tasks,
    MAXIMUM_PERIODIC_TASKS
  );

  const PeriodicTask_t new_task = {
    .C = C_new,
    .T = T_new,
    .D = D_new,
  };

  double U = (double)C_new / T_new;
  for (size_t i = 0; i < existing.count; i++) {
    U += (double)existing.tasks[i].C / existing.tasks[i].T;
  }

  if (U >= 1.0 + EPSILON) {
    return false;
  }

  const double     l_star = calculate_l_star(&new_task, U, &existing);
  const TickType_t H      = compute_hyperperiod(new_task.T, task_view_set);
  const TickType_t D_max  = calculate_d_max(&new_task, &existing);
  const TickType_t upper  = (TickType_t)fmin(H, fmax(D_max, l_star));

  return check_deadlines_edf(&new_task, upper, &existing);
}

/// @brief see if task one is about to add can be added without excessive processor demand;
//         implements theorem 4.6 of Buttazzo's textbook
// NOTE: Admission control test might be conservative as it currently
//       auto-rejects for U = 1 case
// TODO: This fails for U=1.0 in some cases because of floating point imprecision
#if !(USE_MP && USE_PARTITIONED)
bool EDF_can_admit_periodic_task( //
  const TickType_t C_new,
  const TickType_t T_new,
  const TickType_t D_new
) {
  return EDF_can_admit_periodic_task_for_task_set( //
    C_new,
    T_new,
    D_new,
    &periodic_task_view_set
  );
}
#endif // !(USE_MP && USE_PARTITIONED)

#endif // TEST_SUITE == TEST_SUITE_EDF || TEST_SUITE == TEST_SUITE_CBS || TEST_SUITE == TEST_SUITE_PARTITIONED_MP

; // =============================
; // === SRP ADMISSION CONTROL ===
; // =============================

#if USE_SRP

/// @brief Defined in section 7.8.4 in the book Hard Real-Time Computing Systems, Fourth Edition.
static TickType_t calculate_blocking_time(
  const unsigned int target_preemption_level,
  const unsigned int simulated_ceilings[N_RESOURCES],
  const unsigned int new_task_preemption_level,
  const TickType_t   new_task_resource_holds[N_RESOURCES]
) {
  TickType_t max_blocking = 0;

  for (size_t i = 0; i < periodic_task_set.count; i++) {
    if (periodic_tasks[i].preemption_level < target_preemption_level) {
      for (int r = 0; r < N_RESOURCES; r++) {
        if (periodic_tasks[i].resource_hold_times[r] > 0 && simulated_ceilings[r] >= target_preemption_level) {
          max_blocking = MAX(max_blocking, periodic_tasks[i].resource_hold_times[r]);
        }
      }
    }
  }

  if (new_task_preemption_level < target_preemption_level) {
    for (int r = 0; r < N_RESOURCES; r++) {
      if (new_task_resource_holds[r] > 0 && simulated_ceilings[r] >= target_preemption_level) {
        max_blocking = MAX(max_blocking, new_task_resource_holds[r]);
      }
    }
  }
  return max_blocking;
}

/// @brief Processor demand criterion, B(L).
/// Defined in the book Hard Real-Time Computing Systems, Fourth Edition (Eq. 7.24) as the largest amount of time for
/// which a task with relative deadline <= L may be blocked by a task with relative deadline > L.
static TickType_t calculate_B_L(
  const TickType_t   L,
  const unsigned int simulated_ceilings[N_RESOURCES],
  const unsigned int new_preemption_level,
  const TickType_t   new_resource_holds[N_RESOURCES],
  const TickType_t   new_relative_deadline
) {
  unsigned int min_preemption_inside = (unsigned int)-1;
  bool         has_inside_tasks      = false;

  for (size_t i = 0; i < periodic_task_set.count; i++) {
    if (periodic_tasks[i].periodic.relative_deadline <= L) {
      if (periodic_tasks[i].preemption_level < min_preemption_inside) {
        min_preemption_inside = periodic_tasks[i].preemption_level;
      }
      has_inside_tasks = true;
    }
  }

  if (new_relative_deadline <= L) {
    if (new_preemption_level < min_preemption_inside) {
      min_preemption_inside = new_preemption_level;
    }
    has_inside_tasks = true;
  }

  if (!has_inside_tasks)
    return 0;

  TickType_t max_blocking_demand = 0;

  for (size_t i = 0; i < periodic_task_set.count; i++) {
    if (periodic_tasks[i].periodic.relative_deadline > L) {
      for (int r = 0; r < N_RESOURCES; r++) {
        if (periodic_tasks[i].resource_hold_times[r] > 0 && simulated_ceilings[r] >= min_preemption_inside) {
          max_blocking_demand = MAX(max_blocking_demand, periodic_tasks[i].resource_hold_times[r]);
        }
      }
    }
  }

  if (new_relative_deadline > L) {
    for (int r = 0; r < N_RESOURCES; r++) {
      if (new_resource_holds[r] > 0 && simulated_ceilings[r] >= min_preemption_inside) {
        max_blocking_demand = MAX(max_blocking_demand, new_resource_holds[r]);
      }
    }
  }

  return max_blocking_demand;
}

/// @brief Checks if Eq. 7.25 in Hard Real-Time Computing Systems, Fourth Edition holds when introducing a new task to
/// the system.
static bool check_deadlines_srp(
  const PeriodicTask_t *const    new_task,
  const TickType_t               upper,
  const unsigned int             preemption_level,
  const TickType_t               resource_hold_times[N_RESOURCES],
  const unsigned int             simulated_ceilings[N_RESOURCES],
  const PeriodicTaskSet_t *const task_set
) {
  if (new_task == NULL || task_set == NULL || task_set->tasks == NULL) {
    return false;
  }

  for (size_t i = 0; i < task_set->count; i++) {
    const TickType_t Ti = task_set->tasks[i].T;
    const TickType_t Di = task_set->tasks[i].D;

    for (TickType_t k = 0;; k++) {
      const TickType_t t = k * Ti + Di;
      if (t > upper)
        break;

      TickType_t B_t = calculate_B_L(t, simulated_ceilings, preemption_level, resource_hold_times, new_task->D);
      if (dbf(t, new_task, task_set) + (double)B_t > (double)t)
        return false;
    }
  }

  for (TickType_t k = 0;; k++) {
    const TickType_t t = k * new_task->T + new_task->D;
    if (t > upper)
      break;

    TickType_t B_t = calculate_B_L(t, simulated_ceilings, preemption_level, resource_hold_times, new_task->D);
    if (dbf(t, new_task, task_set) + (double)B_t > (double)t)
      return false;
  }

  return true;
}

/// @brief Checks if a system is schedulable when introducing a new task under the SRP scheduling paradigm.
bool SRP_can_admit_periodic_task(
  const TickType_t   completion_time,
  const TickType_t   period,
  const TickType_t   relative_deadline,
  const unsigned int preemption_level,
  const TickType_t  *resource_hold_times
) {
  if (periodic_task_view_set.count >= MAXIMUM_PERIODIC_TASKS) {
    return false;
  }

  const PeriodicTask_t new_task = {
    .C = completion_time,
    .T = period,
    .D = relative_deadline,
  };

  unsigned int simulated_ceilings[N_RESOURCES];
  memcpy(simulated_ceilings, SRP_get_resource_ceilings(), sizeof(simulated_ceilings));
#if N_RESOURCES > 0
  SRP_update_resource_ceilings(preemption_level, resource_hold_times, simulated_ceilings);
#endif // N_RESOURCES > 0

  for (size_t k = 0; k < periodic_task_view_set.count; k++) {
    const TMB_t       *task_k             = periodic_task_view_set.view[k];
    const TickType_t   D_k                = task_k->periodic.relative_deadline;
    const TickType_t   T_k                = task_k->periodic.period;
    const unsigned int preemption_level_k = task_k->preemption_level;

    double sum_U = 0.0;
    for (size_t i = 0; i < periodic_task_view_set.count; i++) {
      const TMB_t *task_i = periodic_task_view_set.view[i];
      if (task_i->periodic.relative_deadline <= D_k) {
        sum_U += (double)task_i->completion_time / task_i->periodic.period;
      }
    }
    if (new_task.D <= D_k) {
      sum_U += (double)new_task.C / new_task.T;
    }

#if N_RESOURCES > 0
    const TickType_t B_k =
      calculate_blocking_time(preemption_level_k, simulated_ceilings, preemption_level, resource_hold_times);
#else  // N_RESOURCES > 0
    const TickType_t B_k = 0;
#endif // N_RESOURCES > 0
    if (sum_U + ((double)B_k / T_k) > 1.0)
      return false;
  }

  double sum_U_new = 0.0;
  for (size_t i = 0; i < periodic_task_view_set.count; i++) {
    const TMB_t *task_i = periodic_task_view_set.view[i];
    if (task_i->periodic.relative_deadline <= relative_deadline) {
      sum_U_new += (double)task_i->completion_time / task_i->periodic.period;
    }
  }
  sum_U_new += (double)new_task.C / new_task.T;

#if N_RESOURCES > 0
  const TickType_t B_new =
    calculate_blocking_time(preemption_level, simulated_ceilings, preemption_level, resource_hold_times);
#else  // N_RESOURCES > 0
  const TickType_t B_new = 0;
#endif // N_RESOURCES > 0
  if (sum_U_new + ((double)B_new / new_task.T) > 1.0) {
    return false;
  }

  double U = (double)new_task.C / new_task.T;
  for (size_t i = 0; i < periodic_task_view_set.count; i++) {
    const TMB_t *task_i = periodic_task_view_set.view[i];
    U += (double)task_i->completion_time / task_i->periodic.period;
  }

  PeriodicTask_t    existing_tasks[MAXIMUM_PERIODIC_TASKS];
  PeriodicTaskSet_t existing = build_periodic_task_set_from_view_set( //
    &periodic_task_view_set,
    existing_tasks,
    MAXIMUM_PERIODIC_TASKS
  );

  const double     l_star = calculate_l_star(&new_task, U, &existing);
  const TickType_t H      = compute_hyperperiod(new_task.T, &periodic_task_view_set);
  const TickType_t D_max  = calculate_d_max(&new_task, &existing);
  const TickType_t upper  = (TickType_t)fmin(H, fmax(D_max, l_star));

#if N_RESOURCES > 0
  return check_deadlines_srp(&new_task, upper, preemption_level, resource_hold_times, simulated_ceilings, &existing);
#else  // N_RESOURCES > 0
  return check_deadlines_srp(&new_task, upper, preemption_level, NULL, NULL, &existing);
#endif // N_RESOURCES > 0
}

#endif // USE_SRP

; // ========================================
; // === GLOBAL SMP EDF ADMISSION CONTROL ===
; // ========================================

#if TEST_SUITE == TEST_SUITE_GLOBAL_MP

/// @brief Calculate total workload that task tau_i can generate in interval [0, L).
/// Formula: W_i(L) = ⌊(L + D_i - C_i) / T_i⌋ * C_i + min(C_i, (L + D_i - C_i) mod T_i)
/// where D_i is relative deadline, T_i is period, C_i is completion time.
static TickType_t workload_in_interval( //
  const TickType_t            L,
  const PeriodicTask_t *const task_i
) {
  configASSERT(task_i != NULL);
  const TickType_t C_i = task_i->C;
  const TickType_t T_i = task_i->T;
  const TickType_t D_i = task_i->D;
  if (L + D_i < C_i) {
    return 0;
  }

  const TickType_t offset  = L + D_i - C_i;
  const TickType_t n_jobs  = offset / T_i;
  const TickType_t partial = offset % T_i;
  const TickType_t carry   = (partial > 0) ? MIN(C_i, partial) : 0;
  return n_jobs * C_i + carry;
}

/// @brief Calculate the EDF-specific interference bound of tau_i on tau_k.
/// Implements Theorem 5 from Bertogna-Cirinei:
/// I_k^i(D_k) = DBF_k^i + min(C_i, max(0, D_k - (num_jobs * T_i)))
static TickType_t edf_interference_bound( //
  const PeriodicTask_t *const task_k,
  const PeriodicTask_t *const task_i
) {
  configASSERT(task_k != NULL);
  configASSERT(task_i != NULL);

  const TickType_t D_k = task_k->D;

  const TickType_t C_i = task_i->C;
  const TickType_t T_i = task_i->T;
  const TickType_t D_i = task_i->D;

  // Calculate the number of full jobs of tau_i that fit within D_k
  // (floor((D_k - D_i) / T_i) + 1)
  // Modified from the paper: if D_i > D_k, cap the num_jobs to zero to prevent underflow/negative results
  const TickType_t num_jobs = (D_i > D_k) ? 0 : ((D_k - D_i) / T_i) + 1;
  const TickType_t dbf_i_k  = num_jobs * C_i;

  // Calculate the carry-in portion: min(C_i, max(0, D_k - num_jobs * T_i))
  const TickType_t sub_term = num_jobs * T_i;
  TickType_t       carry    = 0;

  if (D_k > sub_term) {
    carry = MIN(C_i, D_k - sub_term);
  }

  return dbf_i_k + carry;
}

/// @brief Compute bounded interference of task tau_i on task tau_k over interval of length R.
/// Implements Equation 6 from Bertogna-Cirinei.
/// Returns: min(W_i(R), I_k^i(D_k), R - C_k + 1)
static TickType_t bounded_interference( //
  const TickType_t            R,
  const PeriodicTask_t *const task_k,
  const PeriodicTask_t *const task_i
) {
  configASSERT(task_i != NULL);
  configASSERT(task_k != NULL);

  const TickType_t C_k = task_k->C;

  // Cap: an interfering task cannot cause delay longer than tau_k's wait time
  // Workload Bound: max physical execution time tau_i can request in window R
  // EDF Bound: max interference tau_i can cause under EDF rules
  const TickType_t cap   = (R >= C_k) ? (R - C_k + 1) : 0;
  const TickType_t w_i   = workload_in_interval(R, task_i);
  const TickType_t i_edf = edf_interference_bound(task_k, task_i);

  // Take the minimum of all three
  const TickType_t bound = MIN(w_i, MIN(cap, i_edf));
  return bound;
}

/// @brief Fixed-point iteration to compute response time upper bound for task tau_k.
/// Starting with R_ub = C_k, iteratively compute:
///   R_ub_new = C_k + floor( sum_{i != k} bounded_interference(...) / m )
/// where m is the number of processors.
/// Stops when convergence (R_ub_new == R_ub) or deadline miss (R_ub > D_k).
/// Returns true if R_ub converges and is <= D_k, false otherwise.
static bool response_time_analysis_for_task_in_array(
  const PeriodicTask_t *candidate_tasks,
  const size_t          candidate_count,
  const size_t          task_k_index,
  const size_t          m_processors,
  const TickType_t      max_iterations
) {
  configASSERT(candidate_tasks != NULL);
  configASSERT(task_k_index < candidate_count);

  const PeriodicTask_t task_k = candidate_tasks[task_k_index];

  TickType_t R_ub = task_k.C;
  TickType_t R_ub_prev;

  for (TickType_t iter = 0; iter < max_iterations; iter++) {
    R_ub_prev = R_ub;

    // Accumulate bounded interference from all other tasks in candidate array.
    TickType_t total_interference = 0;

    for (size_t i = 0; i < candidate_count; i++) {
      if (i == task_k_index) {
        continue;
      }
      const TickType_t interference_i = bounded_interference(R_ub, &task_k, &candidate_tasks[i]);
      total_interference += interference_i;
    }

    // Update response time: R_ub_new = C_k + floor(total_interference / m)
    R_ub = task_k.C + (total_interference / m_processors);

    // Check convergence
    if (R_ub == R_ub_prev) {
      return R_ub <= task_k.D;
    }

    // Early exit on deadline miss
    if (R_ub > task_k.D) {
      return false;
    }
  }

  // If iteration limit hit without convergence, be conservative: reject
  return false;
}

bool SMP_can_admit_periodic_task( //
  const TickType_t completion_time,
  const TickType_t period,
  const TickType_t relative_deadline
) {
  // Reject on too many tasks (capacity exceeded)
  if (periodic_task_view_set.count >= MAXIMUM_PERIODIC_TASKS) {
    return false;
  }

  // Reject on invalid parameters
  if (completion_time <= 0 || period <= 0 || relative_deadline <= 0) {
    return false;
  }
  const PeriodicTask_t new_task = {
    .C = completion_time,
    .T = period,
    .D = relative_deadline,
  };

  // Reject if new task has utilization > 1
  if (new_task.C > new_task.T) {
    return false;
  }

  // Get processor count
  const size_t m = (size_t)configNUMBER_OF_CORES;

  // Global utilization check (necessary but not sufficient for global EDF)
  double U = (double)new_task.C / new_task.T;
  for (size_t i = 0; i < periodic_task_view_set.count; i++) {
    const TMB_t *task = periodic_task_view_set.view[i];
    if (task == NULL || task->handle == NULL) {
      continue;
    }
    const double Ci = (double)task->completion_time;
    const double Ti = (double)task->periodic.period;
    U += Ci / Ti;

    // For global EDF on m processors, sufficiency requires U <= m
    if (U > (double)m + EPSILON) {
      return false;
    }
  }

  // Response-Time Analysis on candidate task set
  // Need to check all tasks: both existing and the new one.
  {
    PeriodicTask_t    candidate_tasks[MAXIMUM_PERIODIC_TASKS];
    PeriodicTaskSet_t candidates = build_periodic_task_set_from_view_set( //
      &periodic_task_view_set,
      candidate_tasks,
      MAXIMUM_PERIODIC_TASKS
    );
    if (add_task_to_task_set(&new_task, &candidates) != pdPASS) {
      return false;
    }

    const TickType_t max_iter = 10000;
    for (size_t k = 0; k < candidates.count; k++) {
      if (!response_time_analysis_for_task_in_array( //
            candidate_tasks,
            candidates.count,
            k,
            m,
            max_iter
          )) {
        return false;
      }
    }
  }

  return true;
}

#endif // TEST_SUITE == TEST_SUITE_GLOBAL_MP


#endif // USE_EDF
