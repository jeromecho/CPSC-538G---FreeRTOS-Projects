#ifndef SMP_TESTS_H
#define SMP_TESTS_H

#include "FreeRTOS.h" // IWYU pragma: keep

#if USE_MP

typedef struct {
  TaskFunction_t func; // Function pointer
  TickType_t     C;    // Completion time
  TickType_t     T;    // Period
  TickType_t     D;    // Relative deadline
  uint8_t        core; // Preferred core for the task
} SMP_PeriodicTaskParams_t;

void smp_test_1();
void smp_test_2();
void smp_test_3();

#endif // USE_MP

#endif // SMP_TESTS_H
