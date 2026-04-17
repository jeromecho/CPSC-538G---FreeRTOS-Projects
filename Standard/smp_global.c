#include "smp_global.h"

#if USE_MP && USE_GLOBAL
#include "admission_control.h"
#include "edf_scheduler.h"
#include "tracer.h"

// TODO - might have to modify this to make it more similar to the parititioned
// SMP implementation
BaseType_t SMP_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
) {
#if PERFORM_ADMISSION_CONTROL
  if (!SMP_can_admit_periodic_task(completion_time, period, relative_deadline)) {
    TRACE_record(EVENT_ADMISSION_FAIL(periodic_task_count), TRACE_TASK_PERIODIC, NULL, false);
    TRACE_disable();
    xTaskNotifyGive(monitor_task_handle);
    return pdFALSE;
  }
#endif

  TMB_t     *handle = NULL;
  BaseType_t result = _create_periodic_task_internal(
    task_function,
    task_name,
    periodic_tasks,
    &periodic_task_count,
    // TODO: Decouple SMP private stacks from EDF private stacks (add interface)
    edf_private_stacks_periodic[periodic_task_count],
    completion_time,
    period,
    relative_deadline,
    &handle,
    // TODO: initialize all tasks on core 0 - might have to modify this code so that
    // one iteration of EDF scheduling logic decides cores before the scheduler even
    // starts
    0
  );

  if (result == pdPASS && handle != NULL) {
    if (TMB_handle != NULL) {
      *TMB_handle = handle;
    }
  } else if (result == pdPASS) {
    return pdFAIL;
  }

  return result;
}

static TMB_t *SMP_highest_priority_task_multicore() {
  TMB_t *candidate = NULL;
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    TMB_t *periodic_candidate =
      scheduler_highest_priority_candidate(&periodic_tasks[core], &periodic_task_count[core], NULL);
    if (candidate == NULL || periodic_candidate->absolute_deadline < candidate->absolute_deadline) {
      candidate = periodic_candidate;
    }
  }
  return candidate;
}

TMB_t *SMP_produce_highest_priority_task_not_running(TMB_T **highest_priority_tasks) {
  TMB_t *candidate = NULL;
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    bool task_suspended =
      highest_priority_tasks[core] != NULL && eTaskGetState(highest_priority_tasks[core]->handle) == eSuspended;
    if (
      task_suspended && candidate != NULL &&
      highest_priority_tasks[core]->absolute_deadline < candidate->absolute_deadline
    ) {
      candidate = highest_priority_tasks[core];
    }
  }
  return candidate;
}

void SMP_produce_highest_priority_tasks(TMB_t **highest_priority_tasks) {
  // TOOD - if time, add support for aperiodic tasks
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    *highest_priority_tasks[core] = NULL;
  }
  TMB_t *first_candidate  = NULL;
  TMB_t *second_candidate = NULL;
  first_candidate         = SMP_highest_priority_task_multicore();
  if (first_candidate == NULL) {
    return;
  }
  // HACK: temporarily modify done state of task to prevent selection
  bool original_done_state   = first_candidate->is_done;
  first_candidate->is_done   = false;
  second_candidate           = SMP_highest_priority_task_multicore();
  first_candidate->is_done   = original_done_state;
  *highest_priority_tasks[0] = first_candidate;
  *highest_priority_tasks[1] = second_candidate;
}

#endif // USE_MP && USE_GLOBAL