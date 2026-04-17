#ifndef SCHEDULER_TYPES_H
#define SCHEDULER_TYPES_H

#include "ProjectConfig.h"

#if USE_EDF

#include "FreeRTOS.h" // IWYU pragma: keep
#include "task.h"     // IWYU pragma: keep

#if USE_SRP
// Include TestConfig so that the N_RESOURCES constant is available
#include "config/TestConfig.h" // IWYU pragma: keep
#endif

typedef enum { TASK_PERIODIC, TASK_APERIODIC } TaskType_t;

typedef struct SchedulerParameters {
  TickType_t completion_time;
  void      *parameters_remaining;
} SchedulerParameters_t;

typedef struct TMB_t {
  // --- FreeRTOS-specific data ---
  TaskFunction_t task_function;
  StaticTask_t   task_buffer;
  StackType_t   *stack_buffer;

  // --- Common Metadata ---
  TaskType_t   type;
  size_t       id;        // Index in the corresponding TMB array, starting from 0
  uint32_t     trace_uid; // Stable identity used in trace output and visualisations
  TaskHandle_t handle;
  bool         is_done;
  bool         is_hard_rt;

  // --- Common Scheduling Data ---
  TickType_t release_time;
  TickType_t absolute_deadline;
  TickType_t completion_time;

#if USE_MP
  uint8_t assigned_core;
#endif

  // --- Task Parameters ---
  SchedulerParameters_t parameters;

  // --- Execution Tracking ---
  volatile TickType_t ticks_executed; // Incremented by scheduler in vApplicationTickHook when task is running

  // --- SRP-specific Data ---
#if USE_SRP
  unsigned int preemption_level;
  bool         has_started;
  TickType_t   resource_hold_times[N_RESOURCES];
#endif // USE_SRP

  // --- Type-Specific Data ---
  union {
    struct {
      TickType_t period;
      TickType_t relative_deadline;
      TickType_t next_period;
    } periodic;

    struct {
      // TODO - below field might need to be locked for concurrent scenarios
      bool is_runnable;
    } aperiodic;
  };
} TMB_t;

#endif // USE_EDF

#endif // SCHEDULER_TYPES_H
