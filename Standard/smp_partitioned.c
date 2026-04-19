#include "ProjectConfig.h"

#if USE_MP && USE_PARTITIONED

#include "smp_partitioned.h"

#include "admission_control.h"
#include "helpers.h"
#include "scheduler_internal.h"
#include "tracer.h"

typedef struct {
  TMB_t      *task;
  UBaseType_t core;
  bool        is_periodic;
} SMP_TaskLocation_t;

typedef struct {
  TMB_t **view;
  size_t *count;
  size_t  capacity;
} SMP_ViewSet_t;

#define SMP_MAX_PERIODIC_TASKS_TOTAL  (configNUMBER_OF_CORES * MAXIMUM_PERIODIC_TASKS)
#define SMP_MAX_APERIODIC_TASKS_TOTAL (configNUMBER_OF_CORES * MAXIMUM_APERIODIC_TASKS)

static TMB_t *periodic_sched_view[configNUMBER_OF_CORES][SMP_MAX_PERIODIC_TASKS_TOTAL];
static size_t periodic_sched_count[configNUMBER_OF_CORES] = {0};

static TMB_t g_periodic_snapshot_buffer[SMP_MAX_PERIODIC_TASKS_TOTAL];

static TMB_t *aperiodic_sched_view[configNUMBER_OF_CORES][SMP_MAX_APERIODIC_TASKS_TOTAL];
static size_t aperiodic_sched_count[configNUMBER_OF_CORES] = {0};

static BaseType_t SMP_view_add(TMB_t **view, size_t *count, const size_t capacity, TMB_t *task) {
  if (view == NULL || count == NULL || task == NULL || *count >= capacity) {
    return pdFAIL;
  }

  view[*count] = task;
  (*count)++;
  return pdPASS;
}

static BaseType_t SMP_view_remove(TMB_t **view, size_t *count, TMB_t *task) {
  if (view == NULL || count == NULL || task == NULL) {
    return pdFAIL;
  }

  for (size_t i = 0; i < *count; ++i) {
    if (view[i] == task) {
      const size_t last_index = (*count) - 1;
      if (i != last_index) {
        view[i] = view[last_index];
      }
      view[last_index] = NULL;
      (*count)--;
      return pdPASS;
    }
  }

  return pdFAIL;
}

static SMP_ViewSet_t SMP_get_view_set(const bool is_periodic, const UBaseType_t core) {
  SMP_ViewSet_t set;
  if (is_periodic) {
    set.view     = periodic_sched_view[core];
    set.count    = &periodic_sched_count[core];
    set.capacity = SMP_MAX_PERIODIC_TASKS_TOTAL;
  } else {
    set.view     = aperiodic_sched_view[core];
    set.count    = &aperiodic_sched_count[core];
    set.capacity = SMP_MAX_APERIODIC_TASKS_TOTAL;
  }
  return set;
}

static TMB_t *SMP_find_task_in_view_by_handle(TMB_t *const *view, const size_t count, const TaskHandle_t handle) {
  if (view == NULL || handle == NULL) {
    return NULL;
  }

  for (size_t i = 0; i < count; ++i) {
    TMB_t *candidate = view[i];
    if (candidate != NULL && candidate->handle == handle) {
      return candidate;
    }
  }

  return NULL;
}

static bool SMP_find_task_location(const TaskHandle_t task_handle, SMP_TaskLocation_t *location) {
  if (task_handle == NULL || location == NULL) {
    return false;
  }

  for (UBaseType_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    TMB_t *periodic_match =
      SMP_find_task_in_view_by_handle(periodic_sched_view[core], periodic_sched_count[core], task_handle);
    if (periodic_match != NULL) {
      location->task        = periodic_match;
      location->core        = core;
      location->is_periodic = true;
      return true;
    }

    TMB_t *aperiodic_match =
      SMP_find_task_in_view_by_handle(aperiodic_sched_view[core], aperiodic_sched_count[core], task_handle);
    if (aperiodic_match != NULL) {
      location->task        = aperiodic_match;
      location->core        = core;
      location->is_periodic = false;
      return true;
    }
  }

  return false;
}

static bool SMP_periodic_memory_slot_in_use(const UBaseType_t core, const size_t slot_index) {
  StackType_t *const candidate_stack = private_stacks_periodic[core][slot_index];
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_t *const task = &periodic_tasks[i];
    if (task->handle != NULL && task->assigned_core == core && task->stack_buffer == candidate_stack) {
      return true;
    }
  }
  return false;
}

static BaseType_t SMP_allocate_periodic_memory_slot(const UBaseType_t core, size_t *const slot_index) {
  if (slot_index == NULL) {
    return pdFAIL;
  }

  for (size_t i = 0; i < MAXIMUM_PERIODIC_TASKS; ++i) {
    if (!SMP_periodic_memory_slot_in_use(core, i)) {
      *slot_index = i;
      return pdPASS;
    }
  }

  return pdFAIL;
}

static bool SMP_aperiodic_memory_slot_in_use(const UBaseType_t core, const size_t slot_index) {
  StackType_t *const candidate_stack = private_stacks_aperiodic[core][slot_index];
  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    TMB_t *const task = &aperiodic_tasks[i];
    if (task->handle != NULL && task->assigned_core == core && task->stack_buffer == candidate_stack) {
      return true;
    }
  }
  return false;
}

static BaseType_t SMP_allocate_aperiodic_memory_slot(const UBaseType_t core, size_t *const slot_index) {
  if (slot_index == NULL) {
    return pdFAIL;
  }

  for (size_t i = 0; i < MAXIMUM_APERIODIC_TASKS; ++i) {
    if (!SMP_aperiodic_memory_slot_in_use(core, i)) {
      *slot_index = i;
      return pdPASS;
    }
  }

  return pdFAIL;
}

static BaseType_t SMP_remove_task_at_location(const SMP_TaskLocation_t *location) {
  if (location == NULL || location->task == NULL || location->core >= configNUMBER_OF_CORES) {
    return pdFAIL;
  }

  if (location->task->handle == xTaskGetCurrentTaskHandle()) {
    return pdFAIL;
  }

  const SMP_ViewSet_t set = SMP_get_view_set(location->is_periodic, location->core);
  if (SMP_view_remove(set.view, set.count, location->task) != pdPASS) {
    return pdFAIL;
  }

  TRACE_record(EVENT_BASIC(TRACE_REMOVED_FROM_CORE), TRACE_TASK_EITHER, location->task, false);
  vTaskDelete(location->task->handle);
  location->task->handle         = NULL;
  location->task->is_done        = true;
  location->task->ticks_executed = 0;
  return pdPASS;
}

static size_t SMP_snapshot_periodic_tasks_for_core(const UBaseType_t core) {
  if (SMP_MAX_PERIODIC_TASKS_TOTAL == 0) {
    return 0;
  }

  const size_t count = periodic_sched_count[core];
  for (size_t i = 0; i < count && i < SMP_MAX_PERIODIC_TASKS_TOTAL; ++i) {
    g_periodic_snapshot_buffer[i] = *periodic_sched_view[core][i];
  }
  return (count <= SMP_MAX_PERIODIC_TASKS_TOTAL) ? count : SMP_MAX_PERIODIC_TASKS_TOTAL;
}

static bool SMP_can_admit_periodic_task_on_core( //
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  const UBaseType_t core
) {
  bool can_admit = false;
  taskENTER_CRITICAL();
  const size_t snapshot_count = SMP_snapshot_periodic_tasks_for_core(core);
  can_admit                   = EDF_can_admit_periodic_task_for_task_set( //
    completion_time,
    period,
    relative_deadline,
    g_periodic_snapshot_buffer,
    snapshot_count
  );
  taskEXIT_CRITICAL();
  return can_admit;
}

static bool SMP_can_admit_migrated_periodic_task_on_core(const TMB_t *task, const UBaseType_t core) {
  if (task == NULL) {
    return false;
  }

  bool can_admit = false;
  taskENTER_CRITICAL();
  const size_t snapshot_count = SMP_snapshot_periodic_tasks_for_core(core);
  can_admit                   = EDF_can_admit_periodic_task_for_task_set(
    task->completion_time,
    task->periodic.period,
    task->periodic.relative_deadline,
    g_periodic_snapshot_buffer,
    snapshot_count
  );
  taskEXIT_CRITICAL();
  return can_admit;
}

static TickType_t SMP_calculate_aligned_release_for_migrated_periodic_task(
  const TMB_t *task, const UBaseType_t destination_core, const TickType_t current_tick
) {
  configASSERT(task != NULL);

  const bool scheduler_started = (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);
  if (!scheduler_started) {
    return current_tick;
  }

  TickType_t release_time = current_tick;
  taskENTER_CRITICAL();
  const size_t snapshot_count = SMP_snapshot_periodic_tasks_for_core(destination_core);
  if (snapshot_count > 0) {
    const TickType_t H         = compute_hyperperiod(task->periodic.period, g_periodic_snapshot_buffer, snapshot_count);
    const TickType_t remainder = current_tick % H;
    release_time               = (remainder == 0) ? current_tick : (current_tick + (H - remainder));
  }
  taskEXIT_CRITICAL();

  return release_time;
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

  size_t memory_slot_index = 0;
  if (SMP_allocate_periodic_memory_slot(core, &memory_slot_index) != pdPASS) {
    return pdFAIL;
  }

  // Allocate UID early so we can use it for both failure and success paths
  const uint32_t allocated_uid = allocate_trace_uid();

#if PERFORM_ADMISSION_CONTROL
  if (!SMP_can_admit_periodic_task_on_core(completion_time, period, relative_deadline, core)) {
    admission_control_handle_failure(allocated_uid);
    return pdFALSE;
  }
#endif

  TMB_t     *handle = NULL;
  BaseType_t result = _create_periodic_task_internal(
    task_function,
    task_name,
    periodic_tasks,
    &periodic_task_count,
    private_stacks_periodic[core][memory_slot_index],
    &private_task_buffers_periodic[core][memory_slot_index],
    completion_time,
    period,
    relative_deadline,
    allocated_uid,
    &handle,
    core
  );

  if (result == pdPASS && handle != NULL) {
    if (SMP_view_add(periodic_sched_view[core], &periodic_sched_count[core], SMP_MAX_PERIODIC_TASKS_TOTAL, handle) !=
        pdPASS) {
      vTaskDelete(handle->handle);
      handle->handle = NULL;
      return pdFAIL;
    }
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

  size_t memory_slot_index = 0;
  if (SMP_allocate_aperiodic_memory_slot(core, &memory_slot_index) != pdPASS) {
    return pdFAIL;
  }

  TMB_t     *handle = NULL;
  BaseType_t result = _create_aperiodic_task_internal(
    task_function,
    task_name,
    aperiodic_tasks,
    &aperiodic_task_count,
    private_stacks_aperiodic[core][memory_slot_index],
    &private_task_buffers_aperiodic[core][memory_slot_index],
    completion_time,
    release_time,
    relative_deadline,
    UINT32_MAX,
    &handle,
    NULL,
    true,
    core
  );

  if (result == pdPASS && handle != NULL) {
    if (SMP_view_add(aperiodic_sched_view[core], &aperiodic_sched_count[core], SMP_MAX_APERIODIC_TASKS_TOTAL, handle) !=
        pdPASS) {
      vTaskDelete(handle->handle);
      handle->handle = NULL;
      return pdFAIL;
    }
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

static BaseType_t SMP_move_task_between_core_views(
  TMB_t *task, const bool is_periodic, const UBaseType_t source_core, const UBaseType_t destination_core
) {
  const SMP_ViewSet_t source      = SMP_get_view_set(is_periodic, source_core);
  const SMP_ViewSet_t destination = SMP_get_view_set(is_periodic, destination_core);

  if (SMP_view_add(destination.view, destination.count, destination.capacity, task) != pdPASS) {
    return pdFAIL;
  }

  if (SMP_view_remove(source.view, source.count, task) != pdPASS) {
    (void)SMP_view_remove(destination.view, destination.count, task);
    return pdFAIL;
  }

  return pdPASS;
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

  TMB_t           *task         = location.task;
  const TickType_t current_tick = xTaskGetTickCount();

#if PERFORM_ADMISSION_CONTROL
  if (location.is_periodic && !SMP_can_admit_migrated_periodic_task_on_core(task, destination_core)) {
    return pdFAIL;
  }
#endif

  TickType_t release_time = current_tick;
  if (location.is_periodic) {
    release_time = SMP_calculate_aligned_release_for_migrated_periodic_task(task, destination_core, current_tick);
  }


  if (SMP_move_task_between_core_views(task, location.is_periodic, location.core, destination_core) != pdPASS) {
    return pdFAIL;
  }

  if (pin_task_to_core(task->handle, destination_core) != pdPASS) {
    return pdFAIL;
  }

  vTaskSuspend(task->handle);
  task->ticks_executed = 0;
  task->assigned_core  = (uint8_t)destination_core;

  if (location.is_periodic) {
    task->release_time         = release_time;
    task->periodic.next_period = release_time + task->periodic.period;
    task->absolute_deadline    = release_time + task->periodic.relative_deadline;
  }

  // If migration lands on the release tick, record release explicitly so traces
  // still contain TRACE_RELEASE even when task selection happens immediately.
  if (task->release_time == current_tick) {
    scheduler_record_release(task);
  }

  TRACE_record(EVENT_BASIC(TRACE_MIGRATED_TO_CORE), TRACE_TASK_EITHER, task, false);

  if (TMB_handle != NULL) {
    *TMB_handle = task;
  }

  return pdPASS;
}

static TMB_t *SMP_highest_priority_candidate_from_view(TMB_t *const *view, const size_t count) {
  const TickType_t current_tick      = xTaskGetTickCountFromISR();
  TMB_t           *candidate         = NULL;
  TickType_t       earliest_deadline = portMAX_DELAY;

  for (size_t i = 0; i < count; ++i) {
    TMB_t *task = view[i];
    if (task == NULL || task->handle == NULL) {
      continue;
    }

    if (task->is_done || current_tick < task->release_time) {
      continue;
    }

    if (task->absolute_deadline < earliest_deadline) {
      candidate         = task;
      earliest_deadline = task->absolute_deadline;
    }
  }

  return candidate;
}

TMB_t *SMP_partitioned_produce_highest_priority_task(const UBaseType_t core) {
  TMB_t *periodic_candidate =
    SMP_highest_priority_candidate_from_view(periodic_sched_view[core], periodic_sched_count[core]);
  TMB_t *aperiodic_candidate =
    SMP_highest_priority_candidate_from_view(aperiodic_sched_view[core], aperiodic_sched_count[core]);

  if (periodic_candidate == NULL && aperiodic_candidate == NULL) {
    return NULL;
  }

  const TickType_t periodic_deadline =
    (periodic_candidate != NULL) ? periodic_candidate->absolute_deadline : portMAX_DELAY;
  const TickType_t aperiodic_deadline =
    (aperiodic_candidate != NULL) ? aperiodic_candidate->absolute_deadline : portMAX_DELAY;

  return (periodic_deadline < aperiodic_deadline) ? periodic_candidate : aperiodic_candidate;
}

typedef void (*SMP_TaskVisitor_t)(const TMB_t *task, TickType_t current_tick);

static void SMP_visit_active_tasks_in_view(TMB_t *const *view, const size_t count, SMP_TaskVisitor_t visitor) {
  if (view == NULL || visitor == NULL) {
    return;
  }

  const TickType_t current_tick = xTaskGetTickCountFromISR();
  for (size_t i = 0; i < count; ++i) {
    const TMB_t *task = view[i];
    if (task == NULL || task->handle == NULL) {
      continue;
    }
    visitor(task, current_tick);
  }
}

static void SMP_visit_active_tasks_for_core(const UBaseType_t core, SMP_TaskVisitor_t visitor) {
  SMP_visit_active_tasks_in_view(periodic_sched_view[core], periodic_sched_count[core], visitor);
  SMP_visit_active_tasks_in_view(aperiodic_sched_view[core], aperiodic_sched_count[core], visitor);
}

static void SMP_check_deadline_for_task(const TMB_t *task, const TickType_t current_tick) {
  if (!task->is_done && current_tick > task->absolute_deadline && task->is_hard_rt) {
    scheduler_register_deadline_miss(task);
  }
}

static void SMP_record_release_for_task(const TMB_t *task, const TickType_t current_tick) {
  if (task->release_time == current_tick) {
    scheduler_record_release(task);
  }
}

void SMP_partitioned_suspend_lower_priority_tasks(const TMB_t *const highest_priority_task, const size_t core) {
  for (size_t i = 0; i < periodic_sched_count[core]; ++i) {
    TMB_t *const task = periodic_sched_view[core][i];
    if (task != NULL && task != highest_priority_task && task->handle != NULL &&
        eTaskGetState(task->handle) != eSuspended) {
      scheduler_suspend_task(task);
    }
  }
  for (size_t i = 0; i < aperiodic_sched_count[core]; ++i) {
    TMB_t *const task = aperiodic_sched_view[core][i];
    if (task != NULL && task != highest_priority_task && task->handle != NULL &&
        eTaskGetState(task->handle) != eSuspended) {
      scheduler_suspend_task(task);
    }
  }
}

void SMP_partitioned_reschedule_periodic_tasks(void) {
  const TickType_t current_tick = xTaskGetTickCountFromISR();
  for (size_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    for (size_t i = 0; i < periodic_sched_count[core]; ++i) {
      TMB_t *const task = periodic_sched_view[core][i];
      if (task == NULL || task->handle == NULL) {
        continue;
      }
      (void)scheduler_release_periodic_job_if_ready(task, current_tick);
    }
  }
}

void SMP_partitioned_check_deadlines(void) {
  for (size_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    SMP_visit_active_tasks_for_core(core, SMP_check_deadline_for_task);
  }
}

void SMP_partitioned_record_releases(void) {
  for (size_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    SMP_visit_active_tasks_for_core(core, SMP_record_release_for_task);
  }
}

void SMP_partitioned_suspend_and_resume_tasks(void) {
  for (UBaseType_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    scheduler_suspend_and_resume_tasks(core);
  }
}

#endif
