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

#if USE_MP
  uint8_t assigned_core;
#endif

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
  TaskFunction_t    task_function,
  const char *const task_name,
  const TaskType_t  type,
  const size_t      id,
  TMB_t *const      new_task,
  const TickType_t  completion_time,
  StackType_t      *stack_buffer,
  StaticTask_t     *task_buffer,
  const UBaseType_t core
);
BaseType_t _create_aperiodic_task_internal(
  TaskFunction_t    task_function,
  const char *const task_name,
  TMB_t             task_array[MAXIMUM_APERIODIC_TASKS],
  size_t *const     task_count,
  StackType_t      *stack_buffer,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  const UBaseType_t core
);
BaseType_t _create_periodic_task_internal(
  TaskFunction_t    task_function,
  const char *const task_name,
  TMB_t             task_array[MAXIMUM_PERIODIC_TASKS],
  size_t *const     task_count,
  StackType_t      *stack_buffer,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  const UBaseType_t core
);

void   scheduler_suspend_task(const TMB_t *const task);
void   scheduler_resume_task(const TMB_t *const task);
void   scheduler_check_deadlines_and_record_releases(const TMB_t *const tasks, const size_t count);
TMB_t *scheduler_highest_priority_candidate(TMB_t *tasks, const size_t count);
TMB_t *scheduler_search_array_for_handle(const TaskHandle_t handle, TMB_t *tasks, const size_t count);
void   scheduler_update_priorities();
void   scheduler_record_release(const TMB_t *const task);
void   scheduler_register_deadline_miss(const TMB_t *const task);
void   scheduler_suspend_lower_priority_tasks(const TMB_t *const highest_priority_task);
TMB_t *scheduler_produce_highest_priority_task();

BaseType_t pin_task_to_core(const TaskHandle_t task_handle, const UBaseType_t core);

#if USE_MP && USE_PARTITIONED
extern TMB_t  periodic_tasks[configNUMBER_OF_CORES][MAXIMUM_PERIODIC_TASKS];
extern size_t periodic_task_count[configNUMBER_OF_CORES];

extern TMB_t  aperiodic_tasks[configNUMBER_OF_CORES][MAXIMUM_APERIODIC_TASKS];
extern size_t aperiodic_task_count[configNUMBER_OF_CORES];

extern StackType_t private_stacks_periodic[configNUMBER_OF_CORES][MAXIMUM_PERIODIC_TASKS][SHARED_STACK_SIZE];
extern StackType_t private_stacks_aperiodic[configNUMBER_OF_CORES][MAXIMUM_APERIODIC_TASKS][SHARED_STACK_SIZE];
#else // USE_MP && USE_PARTITIONED
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
#endif // USE_MP && USE_PARTITIONED

#endif // SCHEDULER_INTERNAL_H
