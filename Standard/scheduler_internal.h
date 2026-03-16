#ifndef SCHEDULER_INTERNAL_H
#define SCHEDULER_INTERNAL_H

#include "edf_scheduler.h"

BaseType_t _create_task_internal(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TaskType_t  type,
  const size_t      id,
  TMB_t *const      new_task,
  const TickType_t  completion_time,
  StackType_t      *stack_buffer,
  StaticTask_t     *task_buffer
);
BaseType_t _create_aperiodic_task_internal(
  TaskFunction_t    task_function,
  const char *const task_name,
  StackType_t      *stack_buffer,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
);
BaseType_t _create_periodic_task_internal(
  TaskFunction_t    task_function,
  const char *const task_name,
  StackType_t      *stack_buffer,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
);

extern TMB_t  periodic_tasks[MAXIMUM_PERIODIC_TASKS];
extern size_t periodic_task_count;

extern TMB_t  aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
extern size_t aperiodic_task_count;

#if USE_SRP && ENABLE_STACK_SHARING
extern StackType_t shared_stacks[N_PREEMPTION_LEVELS][SHARED_STACK_SIZE];
#else
extern StackType_t edf_private_stacks_periodic[MAXIMUM_PERIODIC_TASKS][SHARED_STACK_SIZE];
extern StackType_t edf_private_stacks_aperiodic[MAXIMUM_APERIODIC_TASKS][SHARED_STACK_SIZE];
#endif // USE_SRP && ENABLE_STACK_SHARING

#endif // SCHEDULER_INTERNAL_H
