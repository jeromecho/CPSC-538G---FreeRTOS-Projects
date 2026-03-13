
#include "admission_control.h"
#include "helpers.h"
#include "scheduler_internal.h" // Gives access to the task arrays
// TODO: is there potential overheaded introduced by import of math.h?
#include "math.h"

/// @brief demand bound function - assumes task set is synchronnized
static double dbf( //
  const TickType_t L,
  const TickType_t C_new,
  const TickType_t T_new,
  const TickType_t D_new
) {
  double demand = 0.0;

  // Existing tasks
  for (size_t i = 0; i < periodic_task_count; i++) {
    const TickType_t Ci = periodic_tasks[i].completion_time;
    const TickType_t Ti = periodic_tasks[i].periodic.period;
    const TickType_t Di = periodic_tasks[i].periodic.relative_deadline;

    demand += (floor((double)(L + Ti - Di) / Ti)) * Ci;
  }

  // New task
  demand += (floor((double)(L + T_new - D_new) / T_new)) * C_new;
  return demand;
}

double calculate_l_star( //
  const TickType_t C_new,
  const TickType_t T_new,
  const TickType_t D_new,
  const double     U
) {
  double numerator = 0.0;

  // Existing tasks
  for (size_t i = 0; i < periodic_task_count; i++) {
    const double Ci = (double)periodic_tasks[i].completion_time;
    const double Ti = (double)periodic_tasks[i].periodic.period;
    const double Di = (double)periodic_tasks[i].periodic.relative_deadline;
    const double Ui = Ci / Ti;

    numerator += (Ti - Di) * Ui;
  }

  // New task
  const double U_new = (double)C_new / T_new;
  numerator += (T_new - D_new) * U_new;

  const double L_star = numerator / (1.0 - U);
  return L_star;
}

TickType_t calculate_d_max(const TickType_t D_new) {
  TickType_t D_max = D_new;
  for (size_t i = 0; i < periodic_task_count; i++) {
    if (periodic_tasks[i].periodic.relative_deadline > D_max) {
      D_max = periodic_tasks[i].periodic.relative_deadline;
    }
  }
  return D_max;
}

/// @brief checks if demand bound functions evaluates to leq L at points of interest
bool check_deadlines( //
  const TickType_t C_new,
  const TickType_t T_new,
  const TickType_t D_new,
  const TickType_t upper
) {
  bool is_schedulable = true;

  // Check deadlines of existing tasks
  for (size_t i = 0; i < periodic_task_count; i++) {
    const TickType_t Ti = periodic_tasks[i].periodic.period;
    const TickType_t Di = periodic_tasks[i].periodic.relative_deadline;

    for (TickType_t k = 0;; k++) {
      const TickType_t t = k * Ti + Di;
      if (t > upper)
        break;

      if (dbf(t, C_new, T_new, D_new) > t) {
        is_schedulable = false;
      }
    }
  }
  /* Check deadlines of new task */
  for (TickType_t k = 0;; k++) {
    const TickType_t t = k * T_new + D_new;
    if (t > upper)
      break;

    if (dbf(t, C_new, T_new, D_new) > t) {
      is_schedulable = false;
    }
  }
  return is_schedulable;
}

/// @brief see if task one is about to add can be added without excessive processor demand;
//         implements theorem 4.6 of Buttazzo's textbook
// NOTE: Admission control test might be conservative as it currently
//       auto-rejects for U = 1 case
// TODO: This fails for U=1.0 in some cases because of floating point imprecision
bool can_admit_periodic_task( //
  const TickType_t C_new,
  const TickType_t T_new,
  const TickType_t D_new
) {
  // Check Utilization Condition
  double U = (double)C_new / T_new;
  for (size_t i = 0; i < periodic_task_count; i++) {
    const double Ci = (double)periodic_tasks[i].completion_time;
    const double Ti = (double)periodic_tasks[i].periodic.period;
    U += Ci / Ti;
  }

  if (U >= 1.0) {
    return false;
  }

  // Check Processor Demand Conditions
  const double     l_star = calculate_l_star(C_new, T_new, D_new, U);
  const TickType_t H      = compute_hyperperiod(T_new, periodic_tasks, periodic_task_count);
  const TickType_t D_max  = calculate_d_max(D_new);
  const TickType_t upper  = (TickType_t)fmin(H, fmax(D_max, l_star));

  const bool is_schedulable = check_deadlines(C_new, T_new, D_new, upper);
  return is_schedulable;
}

/*

Phase 2:
6. Extend `xTaskCreatePeriodic` function to support delaying of added task so that
   it is synchronized with existing task set (if task is admissible)
7. Verify admissibility checking via printfs
8. Turn off prints and verify logic analyzer output

Phase 3:
9.  Turn off admission control briefly (perhaps via a globally defined flag?) and write
    test for overloading the scheduler
10. Extend `taskPeriodicDone` so that it registers deadline misses and restarts system if there is a
   deadline miss


*/
