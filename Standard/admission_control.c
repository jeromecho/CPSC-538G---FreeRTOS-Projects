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

; // ===================================
; // === CONSTANTS ===
; // ===================================

#define EPSILON 1e-9

void admission_control_handle_failure(const size_t task_index) {
  TRACE_record(EVENT_ADMISSION_FAIL(task_index), TRACE_TASK_PERIODIC, NULL, false);
  TRACE_disable();
  xTaskNotifyGive(monitor_task_handle);
}

; // ===================================
; // === LOCAL FUNCTION DECLARATIONS ===
; // ===================================

static double dbf(
  const TickType_t L,
  const TickType_t C_new,
  const TickType_t T_new,
  const TickType_t D_new,
  const TMB_t     *tasks,
  const size_t     count
);
static double calculate_l_star(
  const TickType_t C_new,
  const TickType_t T_new,
  const TickType_t D_new,
  const double     U,
  const TMB_t     *tasks,
  const size_t     count
);
static TickType_t calculate_d_max(const TickType_t relative_deadline, const TMB_t *tasks, const size_t count);

#if TEST_SUITE == TEST_SUITE_EDF || TEST_SUITE == TEST_SUITE_CBS || TEST_SUITE == TEST_SUITE_PARTITIONED_MP
static bool check_deadlines_edf( //
  const TickType_t C_new,
  const TickType_t T_new,
  const TickType_t D_new,
  const TickType_t upper,
  const TMB_t     *tasks,
  const size_t     count
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
  const TickType_t   C_new,
  const TickType_t   T_new,
  const TickType_t   D_new,
  const TickType_t   upper,
  const unsigned int preemption_level,
  const TickType_t   resource_hold_times[N_RESOURCES],
  const unsigned int simulated_ceilings[N_RESOURCES]
);
#endif


; // ==========================================
; // === COMMON HELPER FUNCTIONS (EDF & SRP) ==
; // ==========================================

/// @brief Demand bound function (dbf) - assumes task set is synchronized
static double dbf( //
  const TickType_t L,
  const TickType_t C_new,
  const TickType_t T_new,
  const TickType_t D_new,
  const TMB_t     *tasks,
  const size_t     count
) {
  double demand = 0.0;

  // Existing tasks
  for (size_t i = 0; i < count; i++) {
    configASSERT(tasks[i].type == TASK_PERIODIC);

    const TickType_t Ci = tasks[i].completion_time;
    const TickType_t Ti = tasks[i].periodic.period;
    const TickType_t Di = tasks[i].periodic.relative_deadline;

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
  const TickType_t completion_time,
  const TickType_t period,
  const TickType_t relative_deadline,
  const double     utilization,
  const TMB_t     *tasks,
  const size_t     count
) {
  double numerator = 0.0;

  for (size_t i = 0; i < count; i++) {
    configASSERT(tasks[i].type == TASK_PERIODIC);

    const double Ci = (double)tasks[i].completion_time;
    const double Ti = (double)tasks[i].periodic.period;
    const double Di = (double)tasks[i].periodic.relative_deadline;
    const double Ui = Ci / Ti;

    numerator += (Ti - Di) * Ui;
  }

  const double new_utilization = (double)completion_time / period;
  numerator += (period - relative_deadline) * new_utilization;

  return numerator / (1.0 - utilization);
}

/// @brief Finds the largest relative deadline between the one provided, and the already existing periodic tasks.
static TickType_t calculate_d_max(const TickType_t relative_deadline, const TMB_t *tasks, const size_t count) {
  TickType_t D_max = relative_deadline;
  for (size_t i = 0; i < count; i++) {
    configASSERT(tasks[i].type == TASK_PERIODIC);

    if (tasks[i].periodic.relative_deadline > D_max) {
      D_max = tasks[i].periodic.relative_deadline;
    }
  }
  return D_max;
}

; // ==================================
; // === PURE EDF ADMISSION CONTROL ===
; // ==================================

#if TEST_SUITE == TEST_SUITE_EDF || TEST_SUITE == TEST_SUITE_CBS || TEST_SUITE == TEST_SUITE_PARTITIONED_MP

/// @brief checks if demand bound functions evaluates to leq L at points of interest
static bool check_deadlines_edf(
  const TickType_t C_new,
  const TickType_t T_new,
  const TickType_t D_new,
  const TickType_t upper,
  const TMB_t     *tasks,
  const size_t     count
) {
  for (size_t i = 0; i < count; i++) {
    const TickType_t Ti = tasks[i].periodic.period;
    const TickType_t Di = tasks[i].periodic.relative_deadline;

    for (TickType_t k = 0;; k++) {
      const TickType_t t = k * Ti + Di;
      if (t > upper)
        break;

      if (dbf(t, C_new, T_new, D_new, tasks, count) > (double)t) {
        return false;
      }
    }
  }

  for (TickType_t k = 0;; k++) {
    const TickType_t t = k * T_new + D_new;
    if (t > upper)
      break;

    if (dbf(t, C_new, T_new, D_new, tasks, count) > (double)t) {
      return false;
    }
  }

  return true;
}

bool EDF_can_admit_periodic_task_for_task_set( //
  const TickType_t C_new,
  const TickType_t T_new,
  const TickType_t D_new,
  const TMB_t     *tasks,
  const size_t     task_count
) {
  double U = (double)C_new / T_new;
  for (size_t i = 0; i < task_count; i++) {
    const double Ci = (double)tasks[i].completion_time;
    const double Ti = (double)tasks[i].periodic.period;
    U += Ci / Ti;
  }

  if (U >= 1.0 + EPSILON) {
    return false;
  }

  const double     l_star = calculate_l_star(C_new, T_new, D_new, U, tasks, task_count);
  const TickType_t H      = compute_hyperperiod(T_new, tasks, task_count);
  const TickType_t D_max  = calculate_d_max(D_new, tasks, task_count);
  const TickType_t upper  = (TickType_t)fmin(H, fmax(D_max, l_star));

  return check_deadlines_edf(C_new, T_new, D_new, upper, tasks, task_count);
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
    periodic_tasks,
    periodic_task_count
  );
}
#endif // !(USE_MP && USE_PARTITIONED)

#endif // TEST_SUITE == TEST_SUITE_EDF

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

  for (size_t i = 0; i < periodic_task_count; i++) {
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

  for (size_t i = 0; i < periodic_task_count; i++) {
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

  for (size_t i = 0; i < periodic_task_count; i++) {
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
  const TickType_t   C_new,
  const TickType_t   T_new,
  const TickType_t   D_new,
  const TickType_t   upper,
  const unsigned int preemption_level,
  const TickType_t   resource_hold_times[N_RESOURCES],
  const unsigned int simulated_ceilings[N_RESOURCES]
) {
  for (size_t i = 0; i < periodic_task_count; i++) {
    const TickType_t Ti = periodic_tasks[i].periodic.period;
    const TickType_t Di = periodic_tasks[i].periodic.relative_deadline;

    for (TickType_t k = 0;; k++) {
      const TickType_t t = k * Ti + Di;
      if (t > upper)
        break;

      TickType_t B_t = calculate_B_L(t, simulated_ceilings, preemption_level, resource_hold_times, D_new);
      if (dbf(t, C_new, T_new, D_new, periodic_tasks, periodic_task_count) + (double)B_t > (double)t)
        return false;
    }
  }

  for (TickType_t k = 0;; k++) {
    const TickType_t t = k * T_new + D_new;
    if (t > upper)
      break;

    TickType_t B_t = calculate_B_L(t, simulated_ceilings, preemption_level, resource_hold_times, D_new);
    if (dbf(t, C_new, T_new, D_new, periodic_tasks, periodic_task_count) + (double)B_t > (double)t)
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
  unsigned int simulated_ceilings[N_RESOURCES];
  memcpy(simulated_ceilings, SRP_get_resource_ceilings(), sizeof(simulated_ceilings));
#if N_RESOURCES > 0
  SRP_update_resource_ceilings(preemption_level, resource_hold_times, simulated_ceilings);
#endif

  for (size_t k = 0; k < periodic_task_count; k++) {
    const TickType_t   D_k                = periodic_tasks[k].periodic.relative_deadline;
    const TickType_t   T_k                = periodic_tasks[k].periodic.period;
    const unsigned int preemption_level_k = periodic_tasks[k].preemption_level;

    double sum_U = 0.0;
    for (size_t i = 0; i < periodic_task_count; i++) {
      if (periodic_tasks[i].periodic.relative_deadline <= D_k) {
        sum_U += (double)periodic_tasks[i].completion_time / periodic_tasks[i].periodic.period;
      }
    }
    if (relative_deadline <= D_k) {
      sum_U += (double)completion_time / period;
    }

#if N_RESOURCES > 0
    const TickType_t B_k =
      calculate_blocking_time(preemption_level_k, simulated_ceilings, preemption_level, resource_hold_times);
#else
    const TickType_t B_k = 0;
#endif
    if (sum_U + ((double)B_k / T_k) > 1.0)
      return false;
  }

  double sum_U_new = 0.0;
  for (size_t i = 0; i < periodic_task_count; i++) {
    if (periodic_tasks[i].periodic.relative_deadline <= relative_deadline) {
      sum_U_new += (double)periodic_tasks[i].completion_time / periodic_tasks[i].periodic.period;
    }
  }
  sum_U_new += (double)completion_time / period;

#if N_RESOURCES > 0
  const TickType_t B_new =
    calculate_blocking_time(preemption_level, simulated_ceilings, preemption_level, resource_hold_times);
#else
  const TickType_t B_new = 0;
#endif
  if (sum_U_new + ((double)B_new / period) > 1.0) {
    return false;
  }

  double U = (double)completion_time / period;
  for (size_t i = 0; i < periodic_task_count; i++) {
    U += (double)periodic_tasks[i].completion_time / periodic_tasks[i].periodic.period;
  }

  const double l_star =
    calculate_l_star(completion_time, period, relative_deadline, U, periodic_tasks, periodic_task_count);
  const TickType_t H     = compute_hyperperiod(period, periodic_tasks, periodic_task_count);
  const TickType_t D_max = calculate_d_max(relative_deadline, periodic_tasks, periodic_task_count);
  const TickType_t upper = (TickType_t)fmin(H, fmax(D_max, l_star));

#if N_RESOURCES > 0
  return check_deadlines_srp(
    completion_time, period, relative_deadline, upper, preemption_level, resource_hold_times, simulated_ceilings
  );
#else
  return check_deadlines_srp(completion_time, period, relative_deadline, upper, preemption_level, NULL, NULL);
#endif
}

#endif

#if TEST_SUITE == TEST_SUITE_GLOBAL_MP
bool SMP_can_admit_periodic_task(
  const TickType_t completion_time, const TickType_t period, const TickType_t relative_deadline
) {
  return true; // TODO: implement
}
#endif         // TEST_SUITE == TEST_SUITE_GLOBAL_MP
