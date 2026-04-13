#ifndef SRP_TESTS_H
#define SRP_TESTS_H

#include "FreeRTOS.h" // IWYU pragma: keep
#include "ProjectConfig.h"

#if USE_SRP

typedef struct {
  TaskFunction_t func;                   // Function pointer
  TickType_t     C;                      // Completion time
  TickType_t     T;                      // Period
  TickType_t     D;                      // Relative deadline
  UBaseType_t    plvl;                   // Preemption Level
  TickType_t     resources[N_RESOURCES]; // Hold times for different resources
} SRP_PeriodicTaskParams_t;

typedef struct {
  TaskFunction_t func;                   // Function pointer
  TickType_t     C;                      // Completion time
  TickType_t     r;                      // Release time
  TickType_t     D;                      // Relative deadline
  UBaseType_t    plvl;                   // Preemption Level
  TickType_t     resources[N_RESOURCES]; // Hold times for different resources
} SRP_AperiodicTaskParams_t;

typedef enum { TASK_TAKE_SEMAPHORE, TASK_GIVE_SEMAPHORE, TASK_EXECUTE } TaskAction_t;
typedef struct {
  TaskAction_t action;
  int          value;
} TaskStep_t;

void srp_test_1();
void srp_test_2();
void srp_test_3();
void srp_test_4();
void srp_test_5();
void srp_test_6();
void srp_test_7();
void srp_test_8();
void srp_test_9();

#endif // USE_SRP

#endif // SRP_TESTS_H
