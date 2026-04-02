#ifndef SCHEDULER_INTERNAL_H
#define SCHEDULER_INTERNAL_H

#include "FreeRTOS.h"
#include "ProjectConfig.h"
#include "task.h"

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
    } aperiodic;
  };
} TMB_t;

BaseType_t _create_task_internal(
  TaskFunction_t        task_function,
  const char *const     task_name,
  const TaskType_t      type,
  const size_t          id,
  TMB_t *const          new_task,
  SchedulerParameters_t parameters,
  StackType_t          *stack_buffer,
  StaticTask_t         *task_buffer
);
BaseType_t _create_aperiodic_task_internal(
  TaskFunction_t    task_function,
  const char *const task_name,
  StackType_t      *stack_buffer,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  void             *parameters_remaining
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
