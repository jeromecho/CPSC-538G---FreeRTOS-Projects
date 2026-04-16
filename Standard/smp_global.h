#ifndef SMP_GLOBAL_H
#define SMP_GLOBAL_H

#include "ProjectConfig.h"

#include "FreeRTOS.h" // IWYU pragma: keep
#if USE_MP && USE_GLOBAL

#include "scheduler_internal.h"

BaseType_t SMP_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
);

#endif // USE_MP

#endif // SMP_GLOBAL_H
