#include "ProjectConfig.h"

#if USE_MP && USE_PARTITIONED

#include "smp_partitioned.h"

#include "admission_control.h"
#include "scheduler_internal.h"
#include "tracer.h"

typedef struct {
  TMB_t      *task;
  UBaseType_t core;
  size_t      index;
  bool        is_periodic;
} SMP_TaskLocation_t;

static bool SMP_find_task_location(const TaskHandle_t task_handle, SMP_TaskLocation_t *location) {
  if (task_handle == NULL || location == NULL) {
    return false;
  }

  for (UBaseType_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    TMB_t *periodic_match =
      scheduler_search_array_for_handle(task_handle, periodic_tasks[core], periodic_task_count[core]);
    if (periodic_match != NULL) {
      location->task        = periodic_match;
      location->core        = core;
      location->index       = periodic_match->id;
      location->is_periodic = true;
      return true;
    }

    TMB_t *aperiodic_match =
      scheduler_search_array_for_handle(task_handle, aperiodic_tasks[core], aperiodic_task_count[core]);
    if (aperiodic_match != NULL) {
      location->task        = aperiodic_match;
      location->core        = core;
      location->index       = aperiodic_match->id;
      location->is_periodic = false;
      return true;
    }
  }

  return false;
}

static BaseType_t SMP_remove_task_at_location(const SMP_TaskLocation_t *location) {
  if (location == NULL || location->task == NULL || location->core >= configNUMBER_OF_CORES) {
    return pdFAIL;
  }

  if (location->task->handle == xTaskGetCurrentTaskHandle()) {
    return pdFAIL;
  }

  if (location->is_periodic) {
    if (location->index >= periodic_task_count[location->core]) {
      return pdFAIL;
    }

    TMB_t  *tasks               = periodic_tasks[location->core];
    size_t *count               = &periodic_task_count[location->core];
    size_t  last_index          = (*count) - 1;
    TMB_t  *task_to_delete      = &tasks[location->index];
    TMB_t  *task_to_move_if_any = &tasks[last_index];

    TRACE_record(EVENT_BASIC(TRACE_REMOVED_FROM_CORE), TRACE_TASK_EITHER, task_to_delete, false);

    vTaskDelete(task_to_delete->handle);

    if (location->index != last_index) {
      tasks[location->index]             = *task_to_move_if_any;
      tasks[location->index].id          = location->index;
      tasks[location->index].task_buffer = task_to_move_if_any->task_buffer;
    }

    (*count)--;
    return pdPASS;
  }

#if MAXIMUM_APERIODIC_TASKS > 0
  if (location->index >= aperiodic_task_count[location->core]) {
    return pdFAIL;
  }

  TMB_t  *tasks               = aperiodic_tasks[location->core];
  size_t *count               = &aperiodic_task_count[location->core];
  size_t  last_index          = (*count) - 1;
  TMB_t  *task_to_delete      = &tasks[location->index];
  TMB_t  *task_to_move_if_any = &tasks[last_index];

  TRACE_record(EVENT_BASIC(TRACE_REMOVED_FROM_CORE), TRACE_TASK_EITHER, task_to_delete, false);

  vTaskDelete(task_to_delete->handle);

  if (location->index != last_index) {
    tasks[location->index]             = *task_to_move_if_any;
    tasks[location->index].id          = location->index;
    tasks[location->index].task_buffer = task_to_move_if_any->task_buffer;
  }

  (*count)--;
  return pdPASS;
#else
  return pdFAIL;
#endif
}

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

BaseType_t SMP_remove_task_from_core(const TaskHandle_t task_handle, const UBaseType_t core) {
  if (core >= configNUMBER_OF_CORES || task_handle == NULL) {
    return pdFAIL;
  }

  SMP_TaskLocation_t location;
  if (!SMP_find_task_location(task_handle, &location)) {
    return pdFAIL;
  }

  if (location.core != core) {
    return pdFAIL;
  }

  return SMP_remove_task_at_location(&location);
}

BaseType_t
SMP_migrate_task_to_core(const TaskHandle_t task_handle, const UBaseType_t destination_core, TMB_t **const TMB_handle) {
  if (destination_core >= configNUMBER_OF_CORES || task_handle == NULL) {
    return pdFAIL;
  }

  SMP_TaskLocation_t location;
  if (!SMP_find_task_location(task_handle, &location)) {
    return pdFAIL;
  }

  if (location.core == destination_core) {
    if (TMB_handle != NULL) {
      *TMB_handle = location.task;
    }
    return pdPASS;
  }

  if (location.task->handle == xTaskGetCurrentTaskHandle()) {
    return pdFAIL;
  }

  const char *task_name = pcTaskGetName(location.task->handle);
  if (task_name == NULL) {
    task_name = "SMP Migrated Task";
  }

  TMB_t     *new_task = NULL;
  BaseType_t created  = pdFAIL;

  if (location.is_periodic) {
    created = SMP_create_periodic_task_on_core(
      location.task->task_function,
      task_name,
      location.task->completion_time,
      location.task->periodic.period,
      location.task->periodic.relative_deadline,
      destination_core,
      &new_task
    );
  }
#if MAXIMUM_APERIODIC_TASKS > 0
  else {
    const TickType_t current_tick = xTaskGetTickCount();
    const TickType_t release_time =
      (location.task->release_time > current_tick) ? location.task->release_time : current_tick;
    const TickType_t relative_deadline = location.task->absolute_deadline - location.task->release_time;

    created = SMP_create_aperiodic_task_on_core(
      location.task->task_function,
      task_name,
      location.task->completion_time,
      release_time,
      relative_deadline,
      destination_core,
      &new_task
    );
  }
#endif

  if (created != pdPASS || new_task == NULL) {
    return pdFAIL;
  }

  if (SMP_remove_task_at_location(&location) != pdPASS) {
    (void)SMP_remove_task_from_core(new_task->handle, destination_core);
    return pdFAIL;
  }

  TRACE_record(EVENT_BASIC(TRACE_MIGRATED_TO_CORE), TRACE_TASK_EITHER, new_task, false);

  if (TMB_handle != NULL) {
    *TMB_handle = new_task;
  }

  return pdPASS;
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
