#ifndef EDF_TESTS_H
#define EDF_TESTS_H

#include "FreeRTOS.h" // IWYU pragma: keep

typedef struct {
  TaskFunction_t func; // Function pointer
  TickType_t     C;    // Completion time
  TickType_t     T;    // Period
  TickType_t     D;    // Relative deadline
} EDF_PeriodicTaskParams_t;

typedef struct {
  TaskFunction_t func; // Function pointer
  TickType_t     C;    // Completion time
  TickType_t     r;    // Release time
  TickType_t     D;    // Relative deadline
} EDF_AperiodicTaskParams_t;

void edf_test_1();
void edf_test_2();
void edf_test_3();
void edf_test_4();
void edf_test_5();
void edf_test_6();
void edf_test_7();
void edf_test_8();
void edf_test_9();
void edf_test_10();
void edf_test_11();

#endif // EDF_TESTS_H
