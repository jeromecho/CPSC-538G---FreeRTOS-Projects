#include "smp_global.h"

#if USE_MP && USE_GLOBAL
#include "admission_control.h"
#include "edf_scheduler.h"
#include "tracer.h"
#include <stdio.h>

BaseType_t SMP_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
) {
  const uint32_t allocated_uid = allocate_trace_uid();

#if PERFORM_ADMISSION_CONTROL
  if (!SMP_can_admit_periodic_task(completion_time, period, relative_deadline)) {
    admission_control_handle_failure(allocated_uid);
    return pdFALSE;
  }
#endif

  TMB_t     *handle = NULL;
  // Initialize all cores on core 1 (arbitrary choice)
  size_t     core   = 1;
  BaseType_t result = _create_periodic_task_internal(
    task_function,
    task_name,
    &periodic_task_set,
    &periodic_task_view_set,
    edf_private_stacks_periodic[periodic_task_set.count],
    &parameters_periodic[periodic_task_set.count],
    &edf_private_task_buffers_periodic[periodic_task_set.count],
    completion_time,
    period,
    relative_deadline,
    allocated_uid,
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
}

static bool multicore_stability_comparator(TMB_t *new_task, TMB_t *current_best) {
  if (new_task->absolute_deadline < current_best->absolute_deadline) {
    return true;
  }
  if (new_task->absolute_deadline == current_best->absolute_deadline) {
    bool new_running  = (eTaskGetState(new_task->handle) == eRunning);
    bool best_running = (eTaskGetState(current_best->handle) == eRunning);
    return (new_running && !best_running);
  }

  return false;
}

// TODO - might be able to replace below function call with existing code
static TMB_t *SMP_highest_priority_task_multicore() {
  return scheduler_highest_priority_candidate_ext(&periodic_task_view_set, NULL, multicore_stability_comparator);
}

void SMP_global_produce_highest_priority_tasks(TMB_t **highest_priority_tasks) {
  // TOOD - if time, add support for aperiodic tasks
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    highest_priority_tasks[core] = NULL;
  }
  TMB_t *first_candidate  = NULL;
  TMB_t *second_candidate = NULL;
  first_candidate         = SMP_highest_priority_task_multicore();
  if (first_candidate == NULL) {
    return;
  }
  // HACK: temporarily modify done state of task to prevent selection
  bool original_done_state = first_candidate->is_done;
  first_candidate->is_done = true;
  second_candidate         = SMP_highest_priority_task_multicore();
  first_candidate->is_done = original_done_state;

  highest_priority_tasks[0] = first_candidate;
  highest_priority_tasks[1] = second_candidate;
}

// TODO - add support for aperiodic tasks if time
void SMP_global_check_deadlines(void) { scheduler_check_deadlines(&periodic_task_view_set); }

void SMP_global_record_releases(void) { scheduler_record_releases(&periodic_task_view_set); }

/// @brief produce true if currently running task on core not in the `highest_priority_tasks`
static bool SMP_global_should_context_switch(TMB_t **const highest_priority_tasks, const size_t core) {
  TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandleForCore(core);
  TMB_t       *current_task_tmb    = EDF_get_task_by_handle(current_task_handle);
  if (current_task_tmb != NULL) {
    current_task_handle = current_task_tmb->handle;
  }
  bool no_edf_tasks = true;
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    no_edf_tasks &= highest_priority_tasks[core] == NULL;
  }
  if (no_edf_tasks) {
    // No EDF tasks want to run.
    // We only need to update if an EDF task is currently running and needs to be stopped.
    return (current_task_tmb != NULL);
  }
  // If no EDF task is currently running (idle/system task is running),
  // and an EDF task is ready, we must update priorities to dispatch it.
  if (current_task_tmb == NULL) {
    return true;
  }
  // if currently running task is one of the 2 highest priority tasks, no need to context switch
  if (current_task_tmb == highest_priority_tasks[0] || current_task_tmb == highest_priority_tasks[1]) {
    return false;
  }
  // Prevent context switch between two tasks with the same deadline for hard-real time tasks
  // NB: Design decision was made to match textbook expected traces and RTSim expected traces respectively
  bool equal_deadlines = true;
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    if (highest_priority_tasks[core] != NULL) {
      equal_deadlines &= (current_task_tmb->absolute_deadline == highest_priority_tasks[core]->absolute_deadline);
    }
  }
  if (equal_deadlines && !current_task_tmb->is_done) {
    return false;
  }
  bool context_switch = false;
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    if (highest_priority_tasks[core] != NULL) {
      context_switch |= highest_priority_tasks[core]->handle != current_task_handle;
    }
  }

  return context_switch;
}

static void SMP_global_mark_highest_priority_tasks(TMB_t *target_task, TMB_t **highest_priority_tasks) {
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    if (highest_priority_tasks[core] == target_task) {
      highest_priority_tasks[core] = NULL;
    }
  }
}

/**
 * @brief get the target task to run for this particular core, given the current list of unscheduled highest priority
 * tasks
 */
static TMB_t *SMP_global_get_target_task(TMB_t **highest_priority_tasks) {
  TMB_t *candidate = NULL;
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    bool task_suspended =
      highest_priority_tasks[core] != NULL && eTaskGetState(highest_priority_tasks[core]->handle) == eSuspended;

    if (candidate == NULL && task_suspended) {
      candidate = highest_priority_tasks[core];
    } else if (
      task_suspended && candidate != NULL &&
      highest_priority_tasks[core]->absolute_deadline < candidate->absolute_deadline
    ) {
      candidate = highest_priority_tasks[core];
    }
  }
  return candidate;
}

// TODO - add support for aperiodic tasks if time
static void SMP_global_suspend_lower_priority_tasks(TMB_t **highest_priority_tasks) {
  for (size_t i = 0; i < periodic_task_view_set.count; ++i) {
    TMB_t *const task = periodic_task_view_set.view[i];
    if (task == NULL || task->handle == NULL) {
      continue;
    }
    if (
      !(task == highest_priority_tasks[0] || task == highest_priority_tasks[1]) &&
      eTaskGetState(task->handle) != eSuspended
    ) {
      scheduler_suspend_task(task);
    }
  }
}

static size_t _len_highest_priority_tasks(TMB_t **highest_priority_tasks) {
  size_t count = 0;
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    count += (highest_priority_tasks[core] != NULL);
  }
  return count;
}

void SMP_global_suspend_and_resume_tasks(void) {
  TMB_t *highest_priority_tasks[configNUMBER_OF_CORES] = {NULL, NULL};
  SMP_global_produce_highest_priority_tasks(highest_priority_tasks);
  SMP_global_suspend_lower_priority_tasks(highest_priority_tasks);
  for (UBaseType_t core = 0; core < configNUMBER_OF_CORES; core++) {
    const bool should_update = SMP_global_should_context_switch(highest_priority_tasks, core);
    TickType_t count         = xTaskGetTickCount();
    if (!should_update) {
      continue;
    }
    // TODO (nice-to-have) - try to centralize calls to TRACE_RECORD
    TRACE_record(EVENT_BASIC(TRACE_PREPARING_CONTEXT_SWITCH), TRACE_TASK_SYSTEM, NULL, true);
    TMB_t *target_task = SMP_global_get_target_task(highest_priority_tasks);
    // If target_task is null, there are no schedulable tasks and we run idle task
    if (target_task != NULL) {
      const UBaseType_t core_affinity_mask = ((UBaseType_t)1U) << core;
      scheduler_resume_task(target_task);
      SMP_global_mark_highest_priority_tasks(target_task, highest_priority_tasks);
    }
  }
}

void SMP_global_reschedule_periodic_tasks() {
  for (size_t i = 0; i < periodic_task_view_set.count; ++i) {
    TMB_t *const task = periodic_task_view_set.view[i];
    if (task == NULL || task->handle == NULL) {
      continue;
    }
    const TickType_t current_tick = xTaskGetTickCountFromISR();
    (void)scheduler_release_periodic_job_if_ready(task, current_tick);
  }
}

#endif // USE_MP && USE_GLOBAL
