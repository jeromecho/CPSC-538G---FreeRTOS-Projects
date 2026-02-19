
#include "edf_scheduler.h"
#include "admission_control.h"
// TODO: is there potential overheaded introduced by import of math.h?
#include "math.h"

// TODO: consider extracting gcd and lcm to a more generic helpers file
static TickType_t gcd(TickType_t a, TickType_t b) {
  while (b != 0) {
    TickType_t tmp = b;
    b              = a % b;
    a              = tmp;
  }
  return a;
}

static TickType_t lcm(TickType_t a, TickType_t b) { return (a / gcd(a, b)) * b; }

/// @brief computes hyperperiod between existing periods and period of newly added task
static TickType_t compute_hyperperiod(TickType_t new_period) {
  TickType_t H = new_period;

  for (size_t i = 0; i < periodic_task_count; i++) {
    H = lcm(H, periodic_tasks[i].period);
  }

  return H;
}

/// @brief demand bound function - assumes task set is synchronnized
static double dbf(TickType_t L, TickType_t C_new, TickType_t T_new, TickType_t D_new) {
  double demand = 0.0;
  /* Existing tasks */
  for (size_t i = 0; i < periodic_task_count; i++) {
    TickType_t Ci = periodic_tasks[i].tmb.completion_time;
    TickType_t Ti = periodic_tasks[i].period;
    TickType_t Di = periodic_tasks[i].relative_deadline;

    demand += (floor((double)(L + Ti - Di) / Ti)) * Ci;
  }
  /* New task */
  demand += (floor((double)(L + T_new - D_new) / T_new)) * C_new;
  return demand;
}

double calculate_l_star(TickType_t C_new, TickType_t T_new, TickType_t D_new, double U) {
  double numerator = 0.0;
  /* existing tasks */
  for (size_t i = 0; i < periodic_task_count; i++) {
    double Ci = (double)periodic_tasks[i].tmb.completion_time;
    double Ti = (double)periodic_tasks[i].period;
    double Di = (double)periodic_tasks[i].relative_deadline;
    double Ui = Ci / Ti;

    numerator += (Ti - Di) * Ui;
  }

  /* new task */
  double U_new = (double)C_new / T_new;
  numerator += (T_new - D_new) * U_new;

  double L_star = numerator / (1.0 - U);
  return L_star;
}

TickType_t calculate_d_max(TickType_t D_new) {
  TickType_t D_max = D_new;
  for (size_t i = 0; i < periodic_task_count; i++) {
    if (periodic_tasks[i].relative_deadline > D_max) {
      D_max = periodic_tasks[i].relative_deadline;
    }
  }
  return D_max;
}

/// @brief checks if demand bound functions evaluates to leq L at points of interest
bool check_deadlines(TickType_t C_new, TickType_t T_new, TickType_t D_new, TickType_t upper) {
  bool is_schedulable = true;
  /* Check deadlines of existing tasks */
  for (size_t i = 0; i < periodic_task_count; i++) {
    TickType_t Ti = periodic_tasks[i].period;
    TickType_t Di = periodic_tasks[i].relative_deadline;

    for (TickType_t k = 0;; k++) {

      TickType_t t = k * Ti + Di;
      if (t > upper)
        break;

      if (dbf(t, C_new, T_new, D_new) > t) {
        is_schedulable = false;
      }
    }
  }
  /* Check deadlines of new task */
  for (TickType_t k = 0;; k++) {

    TickType_t t = k * T_new + D_new;
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
bool can_admit_periodic_task(TickType_t C_new, TickType_t T_new, TickType_t D_new) {
  // 1. Check Utilization Condition
  double U = (double)C_new / T_new;
  for (size_t i = 0; i < periodic_task_count; i++) {
    double Ci = (double)periodic_tasks[i].tmb.completion_time;
    double Ti = (double)periodic_tasks[i].period;
    U += Ci / Ti;
  }

  if (U >= 1.0) {
    return false;
  }

  // 2. Check Processor Demand Conditions
  double     l_star = calculate_l_star(C_new, T_new, D_new, U);
  TickType_t H      = compute_hyperperiod(T_new);
  TickType_t D_max  = calculate_d_max(D_new);
  TickType_t upper  = (TickType_t)fmin(H, fmax(D_max, l_star));

  bool is_schedulable = check_deadlines(C_new, T_new, D_new, upper);
  return is_schedulable;
}

/*

Phase 2:
5. Add test that drops in a synchronous task while the system is running (admissible) and that
   drops in synchronous task while system is running (inadmissible)
6. Extend `xTaskCreatePeriodic` function to support delaying of added task so that
   it is synchronized with existing task set (if task is admissible)
7. Verify admissibility checking via printfs
8. Turn off prints and verify logic analyzer output

Phase 3:
9.  Turn off admission control briefly (perhaps via a globally defined flag?) and write
    test for overloading the scheduler
10. Extend `taskDone` so that it registers deadline misses and restarts system if there is a
   deadline miss


*/