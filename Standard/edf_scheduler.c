#include "edf_scheduler.h"

#include "ProjectConfig.h"
#include "admission_control.h"
#include "helpers.h"
#include "tracer.h"

#if TRACE_WITH_LOGIC_ANALYZER
#include "hardware/gpio.h"
#include "main_blinky.h"
#endif

#if USE_EDF

#if USE_SRP
#include "srp.h"
#endif // USE_SRP

#if USE_CBS
#include "cbs.h"
#endif // USE_CBS

#if USE_MP && USE_PARTITIONED
#include "smp_partitioned.h"
#endif // USE_MP && USE_PARTITIONED

#include "testing/testing.h"

#include <stdio.h>


; // ==========================
; // === GLOBAL TASK ARRAYS ===
; // ==========================

#if USE_MP && USE_PARTITIONED
TMB_t  periodic_tasks[configNUMBER_OF_CORES][MAXIMUM_PERIODIC_TASKS];
size_t periodic_task_count[configNUMBER_OF_CORES] = {0};

TMB_t  aperiodic_tasks[configNUMBER_OF_CORES][MAXIMUM_APERIODIC_TASKS];
size_t aperiodic_task_count[configNUMBER_OF_CORES] = {0};

StackType_t private_stacks_periodic[configNUMBER_OF_CORES][MAXIMUM_PERIODIC_TASKS][SHARED_STACK_SIZE];
StackType_t private_stacks_aperiodic[configNUMBER_OF_CORES][MAXIMUM_APERIODIC_TASKS][SHARED_STACK_SIZE];
#else // USE_MP && USE_PARTITIONED
TMB_t  periodic_tasks[MAXIMUM_PERIODIC_TASKS];
size_t periodic_task_count = 0;

TMB_t  aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
size_t aperiodic_task_count = 0;

#if !(USE_SRP && ENABLE_STACK_SHARING)
StackType_t edf_private_stacks_periodic[MAXIMUM_PERIODIC_TASKS][SHARED_STACK_SIZE];
StackType_t edf_private_stacks_aperiodic[MAXIMUM_APERIODIC_TASKS][SHARED_STACK_SIZE];
#endif // !(USE_SRP && ENABLE_STACK_SHARING)
#endif // USE_MP && USE_PARTITIONED


; // ===================================
; // === LOCAL FUNCTION DECLARATIONS ===
; // ===================================

static void       scheduler_reschedule_periodic_tasks();
static TMB_t     *candidate_highest_priority(TMB_t *tasks, const size_t count, bool (*is_eligible)(TMB_t *));
static TickType_t calculate_release_time_for_new_task(TickType_t new_period, const TMB_t *tasks, const size_t count);

#if TRACE_WITH_LOGIC_ANALYZER
static void
update_gpio_pin(const TaskHandle_t idle_task_handle, const TaskHandle_t current_task_handle, const bool gpio_state);
#else
static void trace_task_switch(TraceEventType_t switch_event);
#endif // TRACE_WITH_LOGIC_ANALYZER


// ===================================
// === HELPER FUNCTION DEFINITIONS ===
// ===================================
static bool is_aperiodic_ready(TMB_t *t) { return !t->is_done; };

bool scheduler_release_periodic_job_if_ready(TMB_t *task, const TickType_t current_tick) {
  if (task == NULL || task->type != TASK_PERIODIC || !task->is_done || current_tick < task->periodic.next_period) {
    return false;
  }

  task->absolute_deadline = task->periodic.next_period + task->periodic.relative_deadline;
  task->release_time      = task->periodic.next_period;
  task->periodic.next_period += task->periodic.period;
  task->is_done = false;
  return true;
}

; // ==================================
; // === FUNCTION HOOK DECLARATIONS ===
; // ==================================

void vApplicationTickHook(void);
void starting_scheduler(void *);
void task_switched_out(void);
void task_switched_in(void);

; // ================================
; // === API FUNCTION DEFINITIONS ===
; // ================================

/// @brief Return task handle of highest priority task in TMB arrays. Return NULL if none
TMB_t *scheduler_produce_highest_priority_task() {
#if USE_MP && USE_PARTITIONED
  return SMP_partitioned_produce_highest_priority_task((UBaseType_t)portGET_CORE_ID());
#else
  TMB_t *periodic_candidate = scheduler_highest_priority_candidate(periodic_tasks, periodic_task_count, NULL);
  TMB_t *aperiodic_candidate =
    scheduler_highest_priority_candidate(aperiodic_tasks, aperiodic_task_count, is_aperiodic_ready);

  // // Early return if there are no tasks available
  if (periodic_candidate == NULL && aperiodic_candidate == NULL) {
    return NULL;
  }

  const TickType_t periodic_deadline =
    (periodic_candidate != NULL) ? periodic_candidate->absolute_deadline : portMAX_DELAY;
  const TickType_t aperiodic_deadline =
    (aperiodic_candidate != NULL) ? aperiodic_candidate->absolute_deadline : portMAX_DELAY;
  TMB_t *candidate = (periodic_deadline < aperiodic_deadline) ? periodic_candidate : aperiodic_candidate;

  return candidate;
#endif
}

// TODO: This should accept the TMB instead of the task handle.
/// @brief Performs all necessary logic to inform the scheduler that a task has finished its execution. Note that
/// calling this function causes the calling task to be suspended.
void EDF_mark_task_done(TaskHandle_t task_handle) {
  // This call to ENTER_CRITICAL is necessary to prevent a task from getting preempted in the
  // middle of calling this function.
  taskENTER_CRITICAL();

  if (task_handle == NULL) {
    task_handle = xTaskGetCurrentTaskHandle();
  }
  configASSERT(task_handle != NULL);

  TMB_t *const task_tmb = EDF_get_task_by_handle(task_handle);
  configASSERT(task_tmb != NULL);

  task_tmb->is_done        = true;
  task_tmb->ticks_executed = 0;

  // If a periodic task completes on/after its next release boundary in this same tick,
  // release the next job immediately so release tracing is not missed by waiting for next tick hook.
  if (task_tmb->type == TASK_PERIODIC) {
    const TickType_t current_tick = xTaskGetTickCount();
    const bool       released_now = scheduler_release_periodic_job_if_ready(task_tmb, current_tick);
    if (released_now && task_tmb->release_time == current_tick) {
      scheduler_record_release(task_tmb);
    }
  }
  // scheduler_suspend_task(task_tmb);

#if USE_SRP
  task_tmb->has_started = false;
  SRP_pop_ceiling();
#endif // USE_SRP

  TRACE_record(EVENT_BASIC(TRACE_DONE), TRACE_TASK_EITHER, task_tmb, false);

  const size_t core = portGET_CORE_ID();
  scheduler_suspend_and_resume_tasks(core);

  taskEXIT_CRITICAL();
}

/// @brief Creates an actual FreeRTOS task using the provided parameters, and sets common fields in the TMB afterwards.
BaseType_t _create_task_internal(
  TaskFunction_t        task_function,
  const char *const     task_name,
  const TaskType_t      type,
  const size_t          id,
  TMB_t *const          new_task,
  SchedulerParameters_t parameters,
  StackType_t          *stack_buffer,
  StaticTask_t         *task_buffer,
  bool                  is_hard_rt,
  const UBaseType_t     core
) {
  new_task->parameters = parameters;

  TaskHandle_t task_handle = xTaskCreateStatic( //
    task_function,
    task_name,
    SHARED_STACK_SIZE,
    (void *)&new_task->parameters,
    PRIORITY_RUNNING,
    stack_buffer,
    task_buffer
  );
  if (task_handle == NULL) {
    return pdFAIL;
  }

  new_task->handle        = task_handle;
  new_task->task_function = task_function;
  new_task->stack_buffer  = stack_buffer;

  new_task->type = type;
  new_task->id   = id;

  new_task->is_done         = false;
  new_task->is_hard_rt      = is_hard_rt;
  new_task->completion_time = parameters.completion_time;
  new_task->ticks_executed  = 0;

  // Pin task to specified core before any trace events are recorded
#if USE_MP
  new_task->assigned_core = (core < configNUMBER_OF_CORES) ? (uint8_t)core : UINT8_MAX;
  if (core < configNUMBER_OF_CORES) {
    if (pin_task_to_core(task_handle, core) != pdPASS) {
      return pdFAIL;
    }
  }
#else
  (void)core; // Suppress unused parameter warning when USE_MP is not enabled
#endif

  return pdPASS;
}

/// @brief Creates a periodic task and initializes all information the EDF scheduler requires to know about it.
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
) {
  if (*task_count >= MAXIMUM_PERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }
  configASSERT(relative_deadline <= period);

  TMB_t *const new_task = &task_array[*task_count];

  SchedulerParameters_t parameters;
  parameters.completion_time      = completion_time;
  parameters.parameters_remaining = NULL;

  BaseType_t result = _create_task_internal( //
    task_function,
    task_name,
    TASK_PERIODIC,
    *task_count,
    new_task,
    parameters,
    stack_buffer,
    &new_task->task_buffer,
    true,
    core
  );
  if (result != pdPASS) {
    if (TMB_handle != NULL)
      *TMB_handle = NULL;
    return result;
  }
  (*task_count)++;

  new_task->periodic.period            = period;
  new_task->periodic.relative_deadline = relative_deadline;

  const bool scheduler_started = (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);
  if (!scheduler_started) {
    const TickType_t current_tick  = xTaskGetTickCount();
    new_task->release_time         = current_tick;
    new_task->periodic.next_period = current_tick + period;
    new_task->absolute_deadline    = current_tick + relative_deadline;
  } else {
    TickType_t release_time        = calculate_release_time_for_new_task(period, task_array, *task_count);
    new_task->release_time         = release_time;
    new_task->periodic.next_period = release_time + period;
    new_task->absolute_deadline    = release_time + relative_deadline;
  }

  scheduler_suspend_task(new_task);

  if (TMB_handle != NULL) {
    *TMB_handle = new_task;
  }
  return pdPASS;
}

/// @brief Creates an aperiodic task and initializes all information the EDF scheduler requires to know about it.
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
  void             *parameters_remaining,
  bool              is_hard_rt,
  const UBaseType_t core
) {
  if (*task_count >= MAXIMUM_APERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }

  const TickType_t current_tick = xTaskGetTickCount();
  configASSERT(release_time >= current_tick);

  TMB_t *const new_task = &task_array[*task_count];

  SchedulerParameters_t parameters;
  parameters.completion_time      = completion_time;
  parameters.parameters_remaining = parameters_remaining;

  BaseType_t result = _create_task_internal( //
    task_function,
    task_name,
    TASK_APERIODIC,
    *task_count,
    new_task,
    parameters,
    stack_buffer,
    &new_task->task_buffer,
    is_hard_rt,
    core
  );
  if (result != pdPASS) {
    if (TMB_handle != NULL)
      *TMB_handle = NULL;
    return result;
  }
  (*task_count)++;

  new_task->release_time      = release_time;
  new_task->absolute_deadline = release_time + relative_deadline;

  scheduler_suspend_task(new_task);

  if (TMB_handle != NULL) {
    *TMB_handle = new_task;
  }

  return pdPASS;
}

#if TEST_SUITE == TEST_SUITE_EDF || TEST_SUITE == TEST_SUITE_CBS

/// REQUIRES: xDeadlinePeriodic <= xPeriod must hold
BaseType_t EDF_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
) {
#if MAXIMUM_PERIODIC_TASKS > 0
#if PERFORM_ADMISSION_CONTROL
  if (!EDF_can_admit_periodic_task(completion_time, period, relative_deadline)) {
    TRACE_record(EVENT_ADMISSION_FAIL(periodic_task_count), TRACE_TASK_PERIODIC, NULL, false);
    TRACE_disable();
    xTaskNotifyGive(monitor_task_handle);
    return pdFALSE;
  }
#endif // PERFORM_ADMISSION_CONTROL

  return _create_periodic_task_internal(
    task_function,
    task_name,
    periodic_tasks,
    &periodic_task_count,
    edf_private_stacks_periodic[periodic_task_count],
    completion_time,
    period,
    relative_deadline,
    TMB_handle,
    0
  );
#else
  // Fallback if no periodic tasks are allowed in this config
  (void)task_function;
  (void)task_name;
  (void)completion_time;
  (void)period;
  (void)relative_deadline;
  (void)TMB_handle;
  return pdFAIL;
#endif // MAXIMUM_PERIODIC_TASKS > 0
}

// TODO: Implement the xTaskDeleteAperiodic function
BaseType_t EDF_create_aperiodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  void             *parameters_remaining,
  bool              is_hard_rt
) {
#if MAXIMUM_APERIODIC_TASKS > 0
  return _create_aperiodic_task_internal(
    task_function,
    task_name,
    aperiodic_tasks,
    &aperiodic_task_count,
    edf_private_stacks_aperiodic[aperiodic_task_count],
    completion_time,
    release_time,
    relative_deadline,
    TMB_handle,
    parameters_remaining,
    is_hard_rt,
    0
  );
#else
  // Fallback if no aperiodic tasks are allowed in this config
  (void)task_function;
  (void)task_name;
  (void)completion_time;
  (void)release_time;
  (void)relative_deadline;
  (void)TMB_handle;
  return pdFAIL;
#endif // MAXIMUM_APERIODIC_TASKS > 0
}
#endif // TEST_SUITE == TEST_SUITE_EDF

#if USE_MP
/// @brief Assign a task to a specific core
BaseType_t pin_task_to_core(const TaskHandle_t task_handle, const UBaseType_t core) {
  if (task_handle == NULL) {
    return pdFAIL;
  }

#if (configUSE_CORE_AFFINITY == 1)
  if (core >= configNUMBER_OF_CORES) {
    return pdFAIL;
  }

  const UBaseType_t core_affinity_mask = ((UBaseType_t)1U) << core;
  vTaskCoreAffinitySet(task_handle, core_affinity_mask);
  return pdPASS;
#endif

  return pdPASS;
}
#endif

/// @brief Dummy task function for periodic tasks. It will run until it has executed for a number of time
/// slices equal to its completion time, at which point it will mark itself as done and suspend
/// itself. When used for periodic tasks, the scheduler should resume it for its next period.
void EDF_periodic_task(void *pvParameters) {
  const BaseType_t xCompletionTime = *(BaseType_t *)pvParameters;
  TaskStep_t       actions[]       = {};

  for (;;) {
    EXECUTE_WORKLOAD(actions, xCompletionTime);
  }
}

/// @brief Dummy task function for aperiodic tasks. It will run until it has executed for a number of time
/// slices equal to its completion time, at which point it will mark itself as done and suspend
/// itself.
void EDF_aperiodic_task(void *pvParameters) {
  const BaseType_t xCompletionTime = *(BaseType_t *)pvParameters;
  const TaskStep_t actions[0]      = {};

  EXECUTE_WORKLOAD(actions, xCompletionTime);

  vTaskDelete(NULL);
}

TMB_t *scheduler_search_array_for_handle(const TaskHandle_t handle, TMB_t *tasks, const size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (tasks[i].handle == handle) {
      return &tasks[i];
    }
  }
  return NULL;
}

// TODO: Some way of providing the task type to speed up the function, if only looking for periodic tasks or only
// looking for aperiodic tasks?
/// @brief Helper function to get the TMB of a task by its task handle
TMB_t *EDF_get_task_by_handle(const TaskHandle_t task_handle) {
  if (task_handle == NULL) {
    return NULL;
  }

  TMB_t *candidate = NULL;

#if USE_MP && USE_PARTITIONED
  for (size_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    candidate = scheduler_search_array_for_handle(task_handle, periodic_tasks[core], periodic_task_count[core]);
    if (candidate)
      return candidate;

    candidate = scheduler_search_array_for_handle(task_handle, aperiodic_tasks[core], aperiodic_task_count[core]);
    if (candidate)
      return candidate;
  }
#else
  candidate = scheduler_search_array_for_handle(task_handle, periodic_tasks, periodic_task_count);
  if (candidate)
    return candidate;

  candidate = scheduler_search_array_for_handle(task_handle, aperiodic_tasks, aperiodic_task_count);
  if (candidate)
    return candidate;
#endif

  return NULL;
}

void starting_scheduler(void *xIdleTaskHandles) {
  TaskHandle_t *idle_task_handles = (TaskHandle_t *)xIdleTaskHandles;
  (void)idle_task_handles;

#if USE_CBS
  CBS_release_tasks();
#endif // USE_CBS

#if USE_MP && USE_PARTITIONED
  SMP_partitioned_check_deadlines();
  SMP_partitioned_record_releases();
  SMP_partitioned_suspend_and_resume_tasks();
#else
  scheduler_check_deadlines(periodic_tasks, periodic_task_count);
  scheduler_check_deadlines(aperiodic_tasks, aperiodic_task_count);
  scheduler_record_releases(periodic_tasks, periodic_task_count);
  scheduler_record_releases(aperiodic_tasks, aperiodic_task_count);
  scheduler_suspend_and_resume_tasks(0);
#endif
}

; // ==================================
; // === LOCAL FUNCTION DEFINITIONS ===
; // ==================================

/// @brief Re-schedules all periodic tasks whose periods have elapsed, and should run again
static void scheduler_reschedule_periodic_tasks() {
#if USE_MP && USE_PARTITIONED
  SMP_partitioned_reschedule_periodic_tasks();
#else
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_t *const     task         = &periodic_tasks[i];
    const TickType_t current_tick = xTaskGetTickCountFromISR();
    (void)scheduler_release_periodic_job_if_ready(task, current_tick);
  }
#endif
}

/// @brief Suspends a task to prevent it from being run by the FreeRTOS scheduler
void scheduler_suspend_task(const TMB_t *const task) {
  configASSERT(task != NULL);
  configASSERT(task->handle != NULL);

  vTaskSuspend(task->handle);
  TRACE_record(EVENT_BASIC(TRACE_SUSPENDED), TRACE_TASK_EITHER, task, true);
}

/// @brief Resumes a task so it can be run by the FreeRTOS scheduler. In non-MP mode, there should only ever be one
/// non-suspended task at any given time.
void scheduler_resume_task(const TMB_t *const task) {
  configASSERT(task != NULL);
  configASSERT(task->handle != NULL);

  xTaskResumeFromISR(task->handle);
  TRACE_record(EVENT_BASIC(TRACE_RESUMED), TRACE_TASK_EITHER, task, true);
}

/// @brief Loops through all tasks in an array and checks whether they have exceeded their deadline.
void scheduler_check_deadlines(const TMB_t *const tasks, const size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const TMB_t *const task         = &tasks[i];
    const bool         task_done    = task->is_done;
    const TickType_t   current_tick = xTaskGetTickCountFromISR();

    // Checks if the task has missed its deadline
    const bool deadline_missed = (current_tick > task->absolute_deadline);
    if (!task_done && deadline_missed && task->is_hard_rt) {
      scheduler_register_deadline_miss(task);
    }
  }
}

/// @brief Loops through all tasks in an array and records release events for tasks released this tick.
void scheduler_record_releases(const TMB_t *const tasks, const size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const TMB_t *const task         = &tasks[i];
    const TickType_t   current_tick = xTaskGetTickCountFromISR();

    // Checks if the release time for a task has arrived
    const bool released_this_tick = (task->release_time == current_tick);
    if (released_this_tick) {
      scheduler_record_release(task);
    }
  }
}

/// @brief Helper function to find the pending task with the nearest deadline
TMB_t *scheduler_highest_priority_candidate(TMB_t *tasks, const size_t count, bool (*is_eligible)(TMB_t *)) {
  const TickType_t current_tick      = xTaskGetTickCountFromISR();
  TMB_t           *candidate         = NULL;
  TickType_t       earliest_deadline = portMAX_DELAY;

  for (size_t i = 0; i < count; ++i) {
    TMB_t *task = &tasks[i];

    // Skip tasks that are done or haven't been released yet
    if (task->is_done || current_tick < task->release_time || (is_eligible != NULL && !is_eligible(task))) {
      continue;
    }

#if USE_SRP
    // If a task hasn't started yet, its preemption level must be
    // strictly greater than the system ceiling to be eligible.
    const unsigned int global_priority_ceiling = SRP_get_system_ceiling();
    if (!task->has_started && task->preemption_level <= global_priority_ceiling) {
      continue;
    }
#endif // USE_SRP

    if (task->absolute_deadline < earliest_deadline) {
      candidate         = task;
      earliest_deadline = task->absolute_deadline;
    }
  }

  return candidate;
}

/// @brief Lowers the priority of all EDF tasks except the selected highest-priority task.
void scheduler_suspend_lower_priority_tasks(const TMB_t *const highest_priority_task, const size_t core) {
#if USE_MP && USE_PARTITIONED
  for (size_t i = 0; i < periodic_task_count[core]; ++i) {
    TMB_t *const task = &periodic_tasks[core][i];
    if (task != highest_priority_task && eTaskGetState(task->handle) != eSuspended) {
      scheduler_suspend_task(task);
    }
  }

  for (size_t i = 0; i < aperiodic_task_count[core]; ++i) {
    TMB_t *const task = &aperiodic_tasks[core][i];
    if (task != highest_priority_task && eTaskGetState(task->handle) != eSuspended) {
      scheduler_suspend_task(task);
    }
  }
#else
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_t *const task = &periodic_tasks[i];
    if (task != highest_priority_task && eTaskGetState(task->handle) != eSuspended) {
      scheduler_suspend_task(task);
    }
  }

  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    TMB_t *const task = &aperiodic_tasks[i];
    if (task != highest_priority_task && eTaskGetState(task->handle) != eSuspended) {
      scheduler_suspend_task(task);
    }
  }
#endif
}

/// @brief Resumes a task
void scheduler_record_release(const TMB_t *const task) {
  configASSERT(task != NULL);
  configASSERT(task->handle != NULL);

  TRACE_record(EVENT_BASIC(TRACE_RELEASE), TRACE_TASK_EITHER, task, true);
}

/// @brief produce true if currently running task is different from the highest priority task
bool scheduler_should_context_switch(const TMB_t *const highest_priority_task, const size_t core) {
  TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandleForCore(core);
  TMB_t       *current_task_tmb    = EDF_get_task_by_handle(current_task_handle);
  if (current_task_tmb != NULL) {
    current_task_handle = current_task_tmb->handle;
  }

  if (highest_priority_task == NULL) {
    // No EDF tasks want to run.
    // We only need to update if an EDF task is currently running and needs to be stopped.
    return (current_task_tmb != NULL);
  }

  // If no EDF task is currently running (idle/system task is running),
  // and an EDF task is ready, we must update priorities to dispatch it.
  if (current_task_tmb == NULL) {
    return true;
  }

  // Prevent context switch between two tasks with the same deadline for hard-real time tasks
  // NB: Design decision was made to match textbook expected traces and RTSim expected traces respectively
  const bool equal_deadlines = (current_task_tmb->absolute_deadline == highest_priority_task->absolute_deadline);
  if (equal_deadlines && !current_task_tmb->is_done &&
      (current_task_tmb->is_hard_rt && highest_priority_task->is_hard_rt)) {
    return false;
  }

  // An EDF task wants to run. Only return true if it is not already running?
  return (highest_priority_task->handle != current_task_handle);
}

/// @brief Deprioritizes all tasks which are not currently running.
void scheduler_suspend_and_resume_tasks(const size_t core) {
#if USE_MP && USE_PARTITIONED
  TMB_t *const highest_priority_task = SMP_partitioned_produce_highest_priority_task(core);
#else  // USE_MP && USE_PARTITIONED
  TMB_t *const highest_priority_task = scheduler_produce_highest_priority_task();
#endif // USE_MP && USE_PARTITIONED

  const bool should_update = scheduler_should_context_switch(highest_priority_task, core);
  if (!should_update) {
    return;
  }

  TRACE_record(EVENT_BASIC(TRACE_PREPARING_CONTEXT_SWITCH), TRACE_TASK_SYSTEM, NULL, true);
  scheduler_suspend_lower_priority_tasks(highest_priority_task, core);

  // If new_highest_priority_task is NULL, that means there are no schedulable tasks and we should be running the idle
  // task. In that case, we shouldn't set a new highest priority task, and the FreeRTOS scheduler should instead elect
  // to run the Idle task.
  if (highest_priority_task != NULL) {
#if USE_SRP
    if (!highest_priority_task->has_started) {
#if ENABLE_STACK_SHARING // Only do the memory wipe if stack sharing is enabled
      SRP_reset_TCB(highest_priority_task);
#endif                   // ENABLE_STACK_SHARING
      highest_priority_task->has_started = true;
      SRP_push_ceiling(highest_priority_task->preemption_level);
    }
#endif // USE_SRP

    scheduler_resume_task(highest_priority_task);
  }
}

/// @brief Calculates release time for dropped task
static TickType_t
calculate_release_time_for_new_task(const TickType_t new_period, const TMB_t *tasks, const size_t count) {
  const TickType_t H            = compute_hyperperiod(new_period, tasks, count);
  const TickType_t current_tick = xTaskGetTickCount(); // TODO: Should this be xTaskGetTickCountFromISR?

  if (current_tick == 0)
    return 0;
  const TickType_t remainder = current_tick % H;
  if (remainder == 0) {
    return current_tick;
  } else {
    return current_tick + (H - remainder);
  }
}

/// @brief Logic for whatever should happen when a deadline is missed
void scheduler_register_deadline_miss(const TMB_t *const task) {
  TRACE_record(EVENT_BASIC(TRACE_DEADLINE_MISS), TRACE_TASK_EITHER, task, true);
  TRACE_disable();

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(monitor_task_handle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

; // =================================
; // === FUNCTION HOOK DEFINITIONS ===
; // =================================

/// @brief Tick hook to ensure the EDF extension's logic is run before the FreeRTOS scheduler every tick
void vApplicationTickHook(void) {
  // Record execution time for the task that ran this tick
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    const TaskHandle_t current_task_on_core = xTaskGetCurrentTaskHandleForCore(core);
    TMB_t             *current_task         = EDF_get_task_by_handle(current_task_on_core);
    if (current_task != NULL) {
      current_task->ticks_executed += 1;
    }
  }

  scheduler_reschedule_periodic_tasks();

#if USE_CBS
  CBS_release_tasks();
  const TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
  TMB_t             *current_task        = EDF_get_task_by_handle(current_task_handle);
  // INVARIANT: current_task is the task that ran for the last tick
  if (current_task != NULL) {
    CBS_update_budget(current_task);
  }
#endif // USE_CBS

#if USE_MP && USE_PARTITIONED
  SMP_partitioned_check_deadlines();
  SMP_partitioned_record_releases();
  SMP_partitioned_suspend_and_resume_tasks();
#else
  scheduler_check_deadlines(periodic_tasks, periodic_task_count);
  scheduler_check_deadlines(aperiodic_tasks, aperiodic_task_count);
  scheduler_record_releases(periodic_tasks, periodic_task_count);
  scheduler_record_releases(aperiodic_tasks, aperiodic_task_count);
  scheduler_suspend_and_resume_tasks(0);
#endif
}

static void trace_task_switch(TraceEventType_t switch_event) {
  const TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
  configASSERT(current_task_handle != NULL);

  bool current_task_is_idle = false;
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    if (current_task_handle == xTaskGetIdleTaskHandleForCore(core)) {
      current_task_is_idle = true;
    }
  }

#if TRACE_WITH_LOGIC_ANALYZER
  update_gpio_pin(idle_task, current_task_handle, 0);
#else
  if (current_task_is_idle) {
    TRACE_record(EVENT_BASIC(switch_event), TRACE_TASK_IDLE, NULL, true);
    return;
  }

  const TMB_t *const current_task_tmb = EDF_get_task_by_handle(current_task_handle);
  if (current_task_tmb == NULL) {
    TRACE_record(EVENT_BASIC(switch_event), TRACE_TASK_SYSTEM, NULL, true);
    return;
  }

  TRACE_record(EVENT_BASIC(switch_event), TRACE_TASK_EITHER, current_task_tmb, true);
#endif
}

/// @brief Tick hook called whenever a task is switched out by the scheduler.
void task_switched_out(void) { trace_task_switch(TRACE_SWITCH_OUT); }

/// @brief Tick hook called whenever a task is switched in by the scheduler.
void task_switched_in(void) { trace_task_switch(TRACE_SWITCH_IN); }

#if TRACE_WITH_LOGIC_ANALYZER
static void update_gpio_pin( //
  const TaskHandle_t idle_task_handle,
  const TaskHandle_t current_task_handle,
  const bool         gpio_state
) {
  // Toggle the pin for the idle task
  if (current_task_handle == idle_task_handle) {
    gpio_put(mainGPIO_IDLE_TASK, 1);
    return;
  }

  // Case where system task was run
  const TMB_t *current_task_tmb = EDF_get_task_by_handle(current_task_handle);
  if (current_task_tmb == NULL) {
    return;
  }

  const size_t task_count = (current_task_tmb->type == TASK_PERIODIC) ? periodic_task_count : aperiodic_task_count;
  TMB_t       *task_array = (current_task_tmb->type == TASK_PERIODIC) ? periodic_tasks : aperiodic_tasks;
  int          gpio_base =
    (current_task_tmb->type == TASK_PERIODIC) ? mainGPIO_PERIODIC_TASK_BASE : mainGPIO_APERIODIC_TASK_BASE;

  for (size_t i = 0; i < task_count; i++) {
    if (current_task_handle == task_array[i].handle) {
      gpio_put(gpio_base + i, gpio_state);
      break;
    }
  }
}
#endif

; // ============================================
; // === FreeRTOS Static Allocation Callbacks ===
; // ============================================

// Since configSUPPORT_STATIC_ALLOCATION is set to 1, the application must provide an
// implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
// used by the Idle task.
void vApplicationGetIdleTaskMemory(
  StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize
) {
  /* These MUST be declared static so they survive after the function exits */
  static StaticTask_t xIdleTaskTCB;
  static StackType_t  uxIdleTaskStack[configMINIMAL_STACK_SIZE];

  *ppxIdleTaskTCBBuffer   = &xIdleTaskTCB;
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;
  *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

void vApplicationGetPassiveIdleTaskMemory(
  StaticTask_t          **ppxIdleTaskTCBBuffer,
  StackType_t           **ppxIdleTaskStackBuffer,
  configSTACK_DEPTH_TYPE *puxIdleTaskStackSize,
  BaseType_t              xPassiveIdleTaskIndex
) {
  /* Static memory for the passive idle task */
  /* On RP2040, xPassiveIdleTaskIndex will be 0 (representing the 1st passive core) */
  static StaticTask_t xPassiveIdleTaskTCB;
  static StackType_t  uxPassiveIdleTaskStack[configMINIMAL_STACK_SIZE];

  *ppxIdleTaskTCBBuffer   = &xPassiveIdleTaskTCB;
  *ppxIdleTaskStackBuffer = uxPassiveIdleTaskStack;
  *puxIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

// Since configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, the
// application must provide an implementation of vApplicationGetTimerTaskMemory()
// to provide the memory that is used by the Timer service task.
void vApplicationGetTimerTaskMemory(
  StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize
) {
  /* These MUST be declared static so they survive after the function exits */
  static StaticTask_t xTimerTaskTCB;
  static StackType_t  uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

  *ppxTimerTaskTCBBuffer   = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

#endif // USE_EDF
