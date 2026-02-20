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

TickType_t compute_hyperperiod(TickType_t new_period) {
  TickType_t H = new_period;

  for (size_t i = 0; i < periodic_task_count; i++) {
    H = lcm(H, periodic_tasks[i].period);
  }

  return H;
}
