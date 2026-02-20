#include "FreeRTOS.h"

static TickType_t compute_hyperperiod(TickType_t new_period) {
  TickType_t H = new_period;

  for (size_t i = 0; i < periodic_task_count; i++) {
    H = lcm(H, periodic_tasks[i].period);
  }

  return H;
}
