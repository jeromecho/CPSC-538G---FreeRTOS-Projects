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


/**
 * @brief produce highest priority tasks among highest priority tasks that isn't currently running
 */
TMB_t *SMP_produce_highest_priority_task_not_running(TMB_t **highest_priority_tasks);

/**
 * @brief produce configNUMBER_OF_CORES highest priority tasks currently present inside
 *        the scheduler
 */
void SMP_global_produce_highest_priority_tasks(TMB_t **highest_priority_tasks);

void SMP_global_check_deadlines(void);

void SMP_global_record_releases(void);

void SMP_global_suspend_and_resume_tasks(void);

void SMP_global_reschedule_periodic_tasks();

#endif // USE_MP && USE_GLOBAL
#endif // SMP_GLOBAL_H
