#ifndef SCHEDULER_INTERNAL_H
#define SCHEDULER_INTERNAL_H

#include "ProjectConfig.h"

#if USE_EDF

#include "FreeRTOS_include.h"
#include "config/TestConfig.h" // IWYU pragma: keep
#include "types/scheduler_types.h"

BaseType_t _create_task_internal(
  TaskFunction_t        task_function,
  const char *const     task_name,
  const TaskType_t      type,
  const size_t          id,
  const uint32_t        trace_uid_override,
  TMB_t *const          new_task,
  SchedulerParameters_t parameters,
  StackType_t          *stack_buffer,
  StaticTask_t         *task_buffer, // Task Buffers hold the TCBs for a task
  bool                  is_hard_rt,
  const UBaseType_t     core
);
BaseType_t _create_aperiodic_task_internal(
  TaskFunction_t    task_function,
  const char *const task_name,
  TMB_t             task_array[MAXIMUM_APERIODIC_TASKS],
  size_t *const     task_count,
  StackType_t      *stack_buffer,
  StaticTask_t     *task_buffer, // Task Buffers hold the TCBs for a task
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  const uint32_t    trace_uid_override,
  TMB_t **const     TMB_handle,
  void             *parameters_remaining,
  bool              is_hard_rt,
  const UBaseType_t core
);
BaseType_t _create_periodic_task_internal(
  TaskFunction_t    task_function,
  const char *const task_name,
  TMB_t             task_array[MAXIMUM_PERIODIC_TASKS],
  size_t *const     task_count,
  StackType_t      *stack_buffer,
  StaticTask_t     *task_buffer, // Task Buffers hold the TCBs for a task
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  const uint32_t    trace_uid_override,
  TMB_t **const     TMB_handle,
  const UBaseType_t core
);

void   scheduler_suspend_task(const TMB_t *const task);
void   scheduler_resume_task(const TMB_t *const task);
void   scheduler_check_deadlines(const TMB_t *const tasks, const size_t count);
void   scheduler_record_releases(const TMB_t *const tasks, const size_t count);
TMB_t *scheduler_highest_priority_candidate(TMB_t *tasks, const size_t count, bool (*is_eligible)(TMB_t *));
TMB_t *scheduler_search_array_for_handle(const TaskHandle_t handle, TMB_t *tasks, const size_t count);
void   scheduler_suspend_and_resume_tasks(const size_t core);
void   scheduler_record_release(const TMB_t *const task);
void   scheduler_register_deadline_miss(const TMB_t *const task);
bool   scheduler_release_periodic_job_if_ready(TMB_t *task, TickType_t current_tick);
void   scheduler_suspend_lower_priority_tasks(const TMB_t *const highest_priority_task, const size_t core);
TMB_t *scheduler_produce_highest_priority_task();
bool   scheduler_should_context_switch(const TMB_t *const highest_priority_task, const size_t core);

uint32_t allocate_trace_uid(void);

BaseType_t pin_task_to_core(const TaskHandle_t task_handle, const UBaseType_t core);

#if USE_MP && USE_PARTITIONED
extern TMB_t  periodic_tasks[configNUMBER_OF_CORES][MAXIMUM_PERIODIC_TASKS];
extern size_t periodic_task_count[configNUMBER_OF_CORES];

extern TMB_t  aperiodic_tasks[configNUMBER_OF_CORES][MAXIMUM_APERIODIC_TASKS];
extern size_t aperiodic_task_count[configNUMBER_OF_CORES];

extern StackType_t private_stacks_periodic[configNUMBER_OF_CORES][MAXIMUM_PERIODIC_TASKS][SHARED_STACK_SIZE];
extern StackType_t private_stacks_aperiodic[configNUMBER_OF_CORES][MAXIMUM_APERIODIC_TASKS][SHARED_STACK_SIZE];

extern StaticTask_t private_task_buffers_periodic[configNUMBER_OF_CORES][MAXIMUM_PERIODIC_TASKS];
extern StaticTask_t private_task_buffers_aperiodic[configNUMBER_OF_CORES][MAXIMUM_APERIODIC_TASKS];
#else // USE_MP && USE_PARTITIONED
extern TMB_t  periodic_tasks[MAXIMUM_PERIODIC_TASKS];
extern size_t periodic_task_count;

extern TMB_t  aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
extern size_t aperiodic_task_count;

extern StaticTask_t edf_private_task_buffers_periodic[MAXIMUM_PERIODIC_TASKS];
extern StaticTask_t edf_private_task_buffers_aperiodic[MAXIMUM_APERIODIC_TASKS];

#if USE_SRP && ENABLE_STACK_SHARING
extern StackType_t shared_stacks[N_PREEMPTION_LEVELS][SHARED_STACK_SIZE];
#else
extern StackType_t edf_private_stacks_periodic[MAXIMUM_PERIODIC_TASKS][SHARED_STACK_SIZE];
extern StackType_t edf_private_stacks_aperiodic[MAXIMUM_APERIODIC_TASKS][SHARED_STACK_SIZE];
#endif // USE_SRP && ENABLE_STACK_SHARING
#endif // USE_MP && USE_PARTITIONED

#endif // USE_EDF

#endif // SCHEDULER_INTERNAL_H
