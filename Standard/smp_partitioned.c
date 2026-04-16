#include "ProjectConfig.h"

#if USE_MP && USE_PARTITIONED

#include "smp_partitioned.h"

#include "admission_control.h"
#include "scheduler_internal.h"

static bool SMP_can_admit_periodic_task_on_core( //
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  const UBaseType_t core
) {
  return EDF_can_admit_periodic_task_for_task_set( //
    completion_time,
    period,
    relative_deadline,
    periodic_tasks[core],
    periodic_task_count[core]
  );
}

BaseType_t SMP_create_periodic_task_on_core(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  const UBaseType_t core,
  TMB_t **const     TMB_handle
) {
#if MAXIMUM_PERIODIC_TASKS > 0
  configASSERT(core < configNUMBER_OF_CORES);

#if PERFORM_ADMISSION_CONTROL
  if (!SMP_can_admit_periodic_task_on_core(completion_time, period, relative_deadline, core)) {
    admission_control_handle_failure(periodic_task_count[core]);
    return pdFALSE;
  }
#endif

  TMB_t     *handle = NULL;
  BaseType_t result = _create_periodic_task_internal(
    task_function,
    task_name,
    periodic_tasks[core],
    &periodic_task_count[core],
    private_stacks_periodic[core][periodic_task_count[core]],
    completion_time,
    period,
    relative_deadline,
    &handle,
    core
  );

  if (result == pdPASS && handle != NULL) {
    if (TMB_handle != NULL) {
      *TMB_handle = handle;
    }
  } else if (result == pdPASS) {
    return pdFAIL;
  }

  return result;
#else
  (void)task_function;
  (void)task_name;
  (void)completion_time;
  (void)period;
  (void)relative_deadline;
  (void)core;
  (void)TMB_handle;
  return pdFAIL;
#endif
}

BaseType_t SMP_create_aperiodic_task_on_core(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  const UBaseType_t core,
  TMB_t **const     TMB_handle
) {
#if MAXIMUM_APERIODIC_TASKS > 0
  configASSERT(core < configNUMBER_OF_CORES);

  TMB_t     *handle = NULL;
  BaseType_t result = _create_aperiodic_task_internal(
    task_function,
    task_name,
    aperiodic_tasks[core],
    &aperiodic_task_count[core],
    private_stacks_aperiodic[core][aperiodic_task_count[core]],
    completion_time,
    release_time,
    relative_deadline,
    &handle,
    core
  );

  if (result == pdPASS && handle != NULL) {
    if (TMB_handle != NULL) {
      *TMB_handle = handle;
    }
  } else if (result == pdPASS) {
    return pdFAIL;
  }

  return result;
#else
  (void)task_function;
  (void)task_name;
  (void)completion_time;
  (void)release_time;
  (void)relative_deadline;
  (void)core;
  (void)TMB_handle;
  return pdFAIL;
#endif
}

TMB_t *SMP_partitioned_produce_highest_priority_task(const UBaseType_t core) {
  TMB_t *periodic_candidate =
    scheduler_highest_priority_candidate(periodic_tasks[core], periodic_task_count[core], NULL);
  TMB_t *aperiodic_candidate =
    scheduler_highest_priority_candidate(aperiodic_tasks[core], aperiodic_task_count[core], NULL);

  if (periodic_candidate == NULL && aperiodic_candidate == NULL) {
    return NULL;
  }

  const TickType_t periodic_deadline =
    (periodic_candidate != NULL) ? periodic_candidate->absolute_deadline : portMAX_DELAY;
  const TickType_t aperiodic_deadline =
    (aperiodic_candidate != NULL) ? aperiodic_candidate->absolute_deadline : portMAX_DELAY;

  return (periodic_deadline < aperiodic_deadline) ? periodic_candidate : aperiodic_candidate;
}

void SMP_partitioned_reschedule_periodic_tasks(void) {
  const TickType_t current_tick = xTaskGetTickCountFromISR();
  for (size_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    for (size_t i = 0; i < periodic_task_count[core]; ++i) {
      TMB_t *const task = &periodic_tasks[core][i];
      (void)scheduler_release_periodic_job_if_ready(task, current_tick);
    }
  }
}

void SMP_partitioned_check_deadlines(void) {
  for (size_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    scheduler_check_deadlines(periodic_tasks[core], periodic_task_count[core]);
    scheduler_check_deadlines(aperiodic_tasks[core], aperiodic_task_count[core]);
  }
}

void SMP_partitioned_record_releases(void) {
  for (size_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    scheduler_record_releases(periodic_tasks[core], periodic_task_count[core]);
    scheduler_record_releases(aperiodic_tasks[core], aperiodic_task_count[core]);
  }
}

void SMP_partitioned_suspend_and_resume_tasks(void) {
  for (UBaseType_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    scheduler_suspend_and_resume_tasks(core);
  }
}

#endif
