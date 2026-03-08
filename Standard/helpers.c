#include "FreeRTOS.h"
#include "edf_scheduler.h"

TickType_t gcd(TickType_t a, TickType_t b) {
  while (b != 0) {
    TickType_t tmp = b;
    b              = a % b;
    a              = tmp;
  }
  return a;
}

TickType_t lcm(TickType_t a, TickType_t b) { return (a / gcd(a, b)) * b; }

/// @brief Computes hyperperiod between existing periods and period of newly added task
TickType_t compute_hyperperiod(TickType_t new_period) {
  TickType_t H = new_period;

  for (size_t i = 0; i < periodic_task_count; i++) {
    H = lcm(H, periodic_tasks[i].periodic.period);
  }

  return H;
}

/// @brief Simulate work for a number of ticks by busy-waiting
void execute_for_ticks(TickType_t ticks_to_wait) {
  // TickType_t start_time    = xTaskGetTickCount();
  TickType_t previous_tick = xTaskGetTickCount();
  TickType_t waited_time   = 0;
  while (waited_time < ticks_to_wait) {
    TickType_t current_tick = xTaskGetTickCount();
    if (current_tick != previous_tick) {
      waited_time += 1;
      previous_tick = current_tick;
    }
  }
}
