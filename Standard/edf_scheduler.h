#ifndef EDF_SCHEDULER_H
#define EDF_SCHEDULER_H

#include "FreeRTOS.h" // IWYU pragma: keep
#include "scheduler_internal.h"
#include "types/scheduler_types.h"

#if USE_EDF

#define PRIORITY_RUNNING (tskIDLE_PRIORITY + 1)
#define PRIORITY_IDLE    (tskIDLE_PRIORITY)

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
  TMB_t **const     TMB_handle,
  void             *parameters_remaining,
  bool              is_hard_rt
);

void EDF_periodic_task(void *pvParameters);
void EDF_aperiodic_task(void *pvParameters);

TMB_t *EDF_get_task_by_handle(const TaskHandle_t task_handle);
void   EDF_mark_task_done(TaskHandle_t task_handle);

extern TaskHandle_t monitor_task_handle;

#endif // USE_EDF

#endif // EDF_SCHEDULER_H
