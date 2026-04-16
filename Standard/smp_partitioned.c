#include "smp_partitioned.h"

#include "ProjectConfig.h"

#if USE_MP && USE_PARTITIONED

#include "edf_scheduler.h"
#include "helpers.h"
#include "tracer.h"

static bool
SMP_can_admit_periodic_task_on_core(const TickType_t completion_time, const TickType_t period, const UBaseType_t core) {
  double U = (double)completion_time / period;
  for (size_t i = 0; i < periodic_task_count[core]; ++i) {
    const double Ci = (double)periodic_tasks[core][i].completion_time;
    const double Ti = (double)periodic_tasks[core][i].periodic.period;
    U += Ci / Ti;
  }

  return U < 1.0;
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
  if (!SMP_can_admit_periodic_task_on_core(completion_time, period, core)) {
    TRACE_record(EVENT_ADMISSION_FAIL(periodic_task_count[core]), TRACE_TASK_PERIODIC, NULL, false);
    TRACE_disable();
    xTaskNotifyGive(monitor_task_handle);
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
  TRACE_record(EVENT_BASIC(TRACE_PREPARING_CONTEXT_SWITCH), TRACE_TASK_SYSTEM, NULL, true);

  for (UBaseType_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    TMB_t *const highest_priority_task = SMP_partitioned_produce_highest_priority_task(core);

    for (size_t i = 0; i < periodic_task_count[core]; ++i) {
      TMB_t *const task = &periodic_tasks[core][i];
      if (task != highest_priority_task) {
        scheduler_suspend_task(task);
      }
    }

    for (size_t i = 0; i < aperiodic_task_count[core]; ++i) {
      TMB_t *const task = &aperiodic_tasks[core][i];
      if (task != highest_priority_task) {
        scheduler_suspend_task(task);
      }
    }

    if (highest_priority_task != NULL) {
      scheduler_resume_task(highest_priority_task);
    }
  }
}

#endif
