#ifndef EDF_SCHEDULER_H
#define EDF_SCHEDULER_H

#include "FreeRTOS.h" // IWYU pragma: keep
#include "ProjectConfig.h"
#include "task.h"

#define PRIORITY_RUNNING     (tskIDLE_PRIORITY + 2)
#define PRIORITY_NOT_RUNNING (tskIDLE_PRIORITY + 1)
#define PRIORITY_IDLE        (tskIDLE_PRIORITY)

#define errADMISSION_FAILED (-6)

typedef enum { TASK_PERIODIC, TASK_APERIODIC } TaskType_t;

typedef struct TMB_t {
  // --- FreeRTOS-specific data ---
  TaskFunction_t task_function;
  StaticTask_t   task_buffer;
  StackType_t   *stack_buffer;

  // --- Common Metadata ---
  TaskType_t   type;
  size_t       id; // Index in the corresponding TMB array, starting from 0
  TaskHandle_t handle;
  bool         is_done;

  // --- Common Scheduling Data ---
  TickType_t release_time;
  TickType_t absolute_deadline;
  TickType_t completion_time;

  // --- SRP-specific Data ---
#if USE_SRP
  unsigned int preemption_level;
  bool         has_started;
#endif // USE_SRP

  // --- Type-Specific Data ---
  union {
    struct {
      TickType_t period;
      TickType_t relative_deadline;
      TickType_t next_period;
    } periodic;

    struct {
    } aperiodic;
  };
} TMB_t;

BaseType_t EDF_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
);

BaseType_t EDF_create_aperiodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
);

void EDF_periodic_task(void *pvParameters);
void EDF_aperiodic_task(void *pvParameters);

TMB_t *EDF_produce_highest_priority_task();
TMB_t *EDF_get_task_by_handle(TaskHandle_t handle);
void   EDF_mark_task_done(TaskHandle_t task_handle);
void   EDF_scheduler_start();

#endif // EDF_SCHEDULER_H
