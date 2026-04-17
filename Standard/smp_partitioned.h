#ifndef SMP_PARTITIONED_H
#define SMP_PARTITIONED_H

#include "ProjectConfig.h"

#if USE_MP

#include "types/scheduler_types.h"

BaseType_t SMP_create_periodic_task_on_core(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  const UBaseType_t core,
  TMB_t **const     TMB_handle
);

BaseType_t SMP_create_aperiodic_task_on_core(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  const UBaseType_t core,
  TMB_t **const     TMB_handle
);

BaseType_t SMP_remove_task_from_core(const TaskHandle_t task_handle, const UBaseType_t core);

BaseType_t
SMP_migrate_task_to_core(const TaskHandle_t task_handle, const UBaseType_t destination_core, TMB_t **const TMB_handle);

TMB_t *SMP_partitioned_produce_highest_priority_task(const UBaseType_t core);
void   SMP_partitioned_reschedule_periodic_tasks(void);
void   SMP_partitioned_check_deadlines(void);
void   SMP_partitioned_record_releases(void);
void   SMP_partitioned_suspend_and_resume_tasks(void);

#endif // USE_MP

#endif // SMP_PARTITIONED_H
