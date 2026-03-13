#include "helpers.h"
#include "task.h"

TickType_t gcd(TickType_t a, TickType_t b) {
  while (b != 0) {
    const TickType_t tmp = b;
    b                    = a % b;
    a                    = tmp;
  }
  return a;
}

TickType_t lcm(const TickType_t a, const TickType_t b) { return (a / gcd(a, b)) * b; }

/// @brief Computes hyperperiod between existing periods and period of newly added task
TickType_t compute_hyperperiod(const TickType_t new_period, const TMB_t *tasks_array, const size_t array_size) {
  TickType_t H = new_period;

  for (size_t i = 0; i < array_size; i++) {
    H = lcm(H, tasks_array[i].periodic.period);
  }

  return H;
}

/// @brief Simulate work for a number of ticks by busy-waiting
void execute_for_ticks(const TickType_t ticks_to_wait) {
  // TickType_t start_time    = xTaskGetTickCount();
  TickType_t previous_tick = xTaskGetTickCount();
  TickType_t waited_time   = 0;
  while (waited_time < ticks_to_wait) {
    const TickType_t current_tick = xTaskGetTickCount();
    if (current_tick != previous_tick) {
      waited_time += 1;
      previous_tick = current_tick;
    }
  }
}
