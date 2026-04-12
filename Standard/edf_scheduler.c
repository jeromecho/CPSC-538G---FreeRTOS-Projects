#include "edf_scheduler.h"

#include "ProjectConfig.h"
#include "admission_control.h"
#include "helpers.h"
#include "tracer.h"

#if TRACE_WITH_LOGIC_ANALYZER
#include "hardware/gpio.h"
#include "main_blinky.h"
#endif

#if USE_SRP
#include "srp.h"
#endif

#include <stdio.h>

static void pin_task_to_scheduler_core(TaskHandle_t task_handle) {
#if (configUSE_CORE_AFFINITY == 1)
  const UBaseType_t core_affinity_mask = ((UBaseType_t)1U) << configTICK_CORE;
  vTaskCoreAffinitySet(task_handle, core_affinity_mask);
#else
  (void)task_handle;
#endif
}

TMB_t  periodic_tasks[MAXIMUM_PERIODIC_TASKS];
size_t periodic_task_count = 0;

TMB_t  aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
size_t aperiodic_task_count = 0;

#if !(USE_SRP && ENABLE_STACK_SHARING)
StackType_t edf_private_stacks_periodic[MAXIMUM_PERIODIC_TASKS][SHARED_STACK_SIZE];
StackType_t edf_private_stacks_aperiodic[MAXIMUM_APERIODIC_TASKS][SHARED_STACK_SIZE];
#endif

; // ===================================
; // === LOCAL FUNCTION DECLARATIONS ===
; // ===================================

void          reschedule_periodic_tasks();
static TMB_t *candidate_highest_priority(TMB_t *tasks, const size_t count);
static void   set_highest_priority(const TMB_t *const task);
static void   deprioritize_task(const TMB_t *const task);
static void   deprioritize_other_tasks(const TMB_t *const highest_priority_task);
static void   release_task(const TMB_t *const task);
bool          should_update_priorities(const TMB_t *const highest_priority_task);
void          update_priorities();
void          check_deadlines_and_release_times(const TMB_t *const tasks, const size_t count);
TickType_t    calculate_release_time_for_new_task(const TickType_t new_period);
void          deadline_miss(const TMB_t *const task);

#if TRACE_WITH_LOGIC_ANALYZER
void update_gpio_pin(
  const TaskHandle_t idle_task_handle, const TaskHandle_t current_task_handle, const bool gpio_state
);
#else
static void trace_task_switch(TraceEventType_t switch_event);
#endif // TRACE_WITH_LOGIC_ANALYZER


; // ==================================
; // === FUNCTION HOOK DECLARATIONS ===
; // ==================================

void vApplicationTickHook(void);
void task_switched_out(void);
void task_switched_in(void);

; // ================================
; // === API FUNCTION DEFINITIONS ===
; // ================================

/// @brief Return task handle of highest priority task in TMB arrays. Return NULL if none
TMB_t *EDF_produce_highest_priority_task() {
  TMB_t *periodic_candidate  = candidate_highest_priority(periodic_tasks, periodic_task_count);
  TMB_t *aperiodic_candidate = candidate_highest_priority(aperiodic_tasks, aperiodic_task_count);

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
}

// TODO: This should accept the TMB instead of the task handle.
/// @brief Performs all necessary logic to inform the scheduler that a task has finished its execution. Note that
/// calling this function causes the calling task to be suspended.
void EDF_mark_task_done(TaskHandle_t task_handle) {
  // This call to ENTER_CRITICAL is necessary both to prevent race conditions, where a task might get preempted in the
  // middle of calling this function, but also because of the calls to vTaskSuspend and vTaskPrioritySet in the middle
  // of it, which would otherwise cause the scheduler to perform a context switch immediately
  if (task_handle == NULL) {
    task_handle = xTaskGetCurrentTaskHandle();
  }
  configASSERT(task_handle != NULL);

  taskENTER_CRITICAL();

  TMB_t *const task_tmb = EDF_get_task_by_handle(task_handle);
  configASSERT(task_tmb != NULL);

  vTaskSuspend(task_handle); // Suspension is needed so that idle task will run when no tasks are ready
  task_tmb->is_done = true;

#if USE_SRP
  task_tmb->has_started = false;
  SRP_pop_ceiling();
#endif // USE_SRP

  update_priorities();

  TRACE_record(EVENT_BASIC(TRACE_DONE), TRACE_TASK_EITHER, task_tmb);

  taskEXIT_CRITICAL();
}

/// @brief Creates an actual FreeRTOS task using the provided parameters, and sets common fields in the TMB afterwards.
BaseType_t _create_task_internal(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TaskType_t  type,
  const size_t      id,
  TMB_t *const      new_task,
  const TickType_t  completion_time,
  StackType_t      *stack_buffer,
  StaticTask_t     *task_buffer
) {
  TaskHandle_t task_handle = xTaskCreateStatic( //
    task_function,
    task_name,
    SHARED_STACK_SIZE,
    (void *)completion_time,
    PRIORITY_RUNNING,
    stack_buffer,
    task_buffer
  );
  if (task_handle == NULL) {
    return pdFAIL;
  }

  pin_task_to_scheduler_core(task_handle);

  new_task->handle        = task_handle;
  new_task->task_function = task_function;
  new_task->stack_buffer  = stack_buffer;

  new_task->type = type;
  new_task->id   = id;

  new_task->is_done         = false;
  new_task->completion_time = completion_time;

  return pdPASS;
}

/// @brief Creates a periodic task and initializes all information the EDF scheduler requires to know about it.
BaseType_t _create_periodic_task_internal(
  TaskFunction_t    task_function,
  const char *const task_name,
  StackType_t      *stack_buffer,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
) {
  if (periodic_task_count >= MAXIMUM_PERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }
  configASSERT(relative_deadline <= period);

  TMB_t *const new_task = &periodic_tasks[periodic_task_count];

  BaseType_t result = _create_task_internal( //
    task_function,
    task_name,
    TASK_PERIODIC,
    periodic_task_count,
    new_task,
    completion_time,
    stack_buffer,
    &new_task->task_buffer
  );
  if (result != pdPASS) {
    if (TMB_handle != NULL)
      *TMB_handle = NULL;
    return result;
  }
  periodic_task_count++;

  new_task->periodic.period            = period;
  new_task->periodic.relative_deadline = relative_deadline;

  const bool scheduler_started = (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED);
  if (!scheduler_started) {
    const TickType_t current_tick  = xTaskGetTickCount();
    new_task->release_time         = current_tick;
    new_task->periodic.next_period = current_tick + period;
    new_task->absolute_deadline    = current_tick + relative_deadline;
    TRACE_record(EVENT_BASIC(TRACE_RELEASE), TRACE_TASK_PERIODIC, new_task);
  } else {
    TickType_t release_time        = calculate_release_time_for_new_task(period);
    new_task->release_time         = release_time;
    new_task->periodic.next_period = release_time + period;
    new_task->absolute_deadline    = release_time + relative_deadline;
    vTaskSuspend(new_task->handle);
    deprioritize_task(new_task);
  }

  if (TMB_handle != NULL) {
    *TMB_handle = new_task;
  }

  return pdPASS;
}

/// @brief Creates an aperiodic task and initializes all information the EDF scheduler requires to know about it.
BaseType_t _create_aperiodic_task_internal(
  TaskFunction_t    task_function,
  const char *const task_name,
  StackType_t      *stack_buffer,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
) {
  if (aperiodic_task_count >= MAXIMUM_APERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }

  const TickType_t current_tick = xTaskGetTickCount();
  configASSERT(release_time >= current_tick);

  TMB_t *const new_task = &aperiodic_tasks[aperiodic_task_count];

  BaseType_t result = _create_task_internal( //
    task_function,
    task_name,
    TASK_APERIODIC,
    aperiodic_task_count,
    new_task,
    completion_time,
    stack_buffer,
    &new_task->task_buffer
  );
  if (result != pdPASS) {
    if (TMB_handle != NULL)
      *TMB_handle = NULL;
    return result;
  }
  aperiodic_task_count++;

  new_task->release_time      = release_time;
  new_task->absolute_deadline = release_time + relative_deadline;

  if (release_time == current_tick) {
    TRACE_record(EVENT_BASIC(TRACE_RELEASE), TRACE_TASK_APERIODIC, new_task);
  } else {
    vTaskSuspend(new_task->handle);
    deprioritize_task(new_task);
  }

  if (TMB_handle != NULL) {
    *TMB_handle = new_task;
  }

  return pdPASS;
}

#if !USE_SRP
/// REQUIRES: xDeadlinePeriodic <= xPeriod must hold
BaseType_t EDF_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
) {
#if PERFORM_ADMISSION_CONTROL
  if (!EDF_can_admit_periodic_task(completion_time, period, relative_deadline)) {
    TRACE_record(EVENT_ADMISSION_FAIL(periodic_task_count), TRACE_TASK_PERIODIC, NULL);
    TRACE_disable();
    return pdFALSE;
  }
#endif // PERFORM_ADMISSION_CONTROL

  return _create_periodic_task_internal(
    task_function,
    task_name,
    edf_private_stacks_periodic[periodic_task_count],
    completion_time,
    period,
    relative_deadline,
    TMB_handle
  );
}

// TODO: Implement the xTaskDeleteAperiodic function, which will be responsible for deleting
// aperiodic tasks once they are done executing.  This is necessary to prevent memory leaks, since
// aperiodic tasks are not reused like periodic tasks.
BaseType_t EDF_create_aperiodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
) {
  return _create_aperiodic_task_internal( //
    task_function,
    task_name,
    edf_private_stacks_aperiodic[aperiodic_task_count],
    completion_time,
    release_time,
    relative_deadline,
    TMB_handle
  );
}
#endif // !USE_SRP

/// @brief Dummy task function for periodic tasks. It will run until it has executed for a number of time
/// slices equal to its completion time, at which point it will mark itself as done and suspend
/// itself. When used for periodic tasks, the scheduler should resume it for its next period.
void EDF_periodic_task(void *pvParameters) {
  const BaseType_t xCompletionTime = (BaseType_t)pvParameters;

  for (;;) {
    execute_for_ticks(xCompletionTime);
    EDF_mark_task_done(NULL);
  }
}

/// @brief Dummy task function for aperiodic tasks. It will run until it has executed for a number of time
/// slices equal to its completion time, at which point it will mark itself as done and suspend
/// itself.
void EDF_aperiodic_task(void *pvParameters) {
  const BaseType_t xCompletionTime = (BaseType_t)pvParameters;
  execute_for_ticks(xCompletionTime);
  EDF_mark_task_done(NULL);
  vTaskDelete(NULL);
}

// TODO: Some way of providing the task type to speed up the function, if only looking for periodic tasks or only
// looking for aperiodic tasks?
/// @brief Helper function to get the TMB of a task by its task handle
TMB_t *EDF_get_task_by_handle(const TaskHandle_t handle) {
  for (size_t i = 0; i < periodic_task_count; ++i) {
    if (periodic_tasks[i].handle == handle) {
      return &periodic_tasks[i];
    }
  }

  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    if (aperiodic_tasks[i].handle == handle) {
      return &aperiodic_tasks[i];
    }
  }

  return NULL;
}

/// @brief Starts the scheduler. Among other things selects the initial tasks to be run, and then starts the actual
/// FreeRTOS scheduler
void EDF_scheduler_start() {
  printf("Starting scheduler.\n");

  TMB_t *const highest_priority_task = EDF_produce_highest_priority_task();
  const bool   should_update         = should_update_priorities(highest_priority_task);
  if (!should_update) {
    configASSERT(false && "Scheduler should always update priorities when starting up");
  }

  TRACE_record(EVENT_BASIC(TRACE_UPDATING_PRIORITIES), TRACE_TASK_SYSTEM, NULL);

  deprioritize_other_tasks(highest_priority_task);

#if USE_SRP
  if (!highest_priority_task->has_started) {
#if ENABLE_STACK_SHARING // Only do the memory wipe if stack sharing is enabled
    SRP_reset_TCB(highest_priority_task);
#endif                   // ENABLE_STACK_SHARING
    highest_priority_task->has_started = true;
    SRP_push_ceiling(highest_priority_task->preemption_level);
  }
#endif // USE_SRP

  vTaskStartScheduler();
}

; // ==================================
; // === LOCAL FUNCTION DEFINITIONS ===
; // ==================================

/// @brief Re-schedules all periodic tasks whose periods have elapsed, and should run again
void reschedule_periodic_tasks() {
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_t *const     task         = &periodic_tasks[i];
    const TickType_t current_tick = xTaskGetTickCountFromISR();

    if (task->is_done && current_tick >= task->periodic.next_period) {
      task->absolute_deadline = task->periodic.next_period + task->periodic.relative_deadline;
      task->release_time      = task->periodic.next_period;
      task->periodic.next_period += task->periodic.period;
      task->is_done = false;
    }
  }
}

/// @brief Helper function to find the pending task with the nearest deadline
static TMB_t *candidate_highest_priority(TMB_t *tasks, const size_t count) {
  const TickType_t current_tick      = xTaskGetTickCountFromISR();
  TMB_t           *candidate         = NULL;
  TickType_t       earliest_deadline = portMAX_DELAY;

  for (size_t i = 0; i < count; ++i) {
    TMB_t *task = &tasks[i];

    // Skip tasks that are done or haven't been released yet
    if (task->is_done || current_tick < task->release_time) {
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

/// @brief Increases the priority of a task to the highest possible/maximum, which should force it to be chosen to run
/// by the FreeRTOS scheduler
static void set_highest_priority(const TMB_t *const task) {
  configASSERT(task != NULL);
  configASSERT(task->handle != NULL);

  vTaskPrioritySet(task->handle, PRIORITY_RUNNING);

  TRACE_record(EVENT_BASIC(TRACE_PRIORITY_SET), TRACE_TASK_EITHER, task);
}

/// @brief Lowers the priority of a task to the lowest possible/minimum, which should prevent it from running.
static void deprioritize_task(const TMB_t *const task) {
  configASSERT(task != NULL);
  configASSERT(task->handle != NULL);

  vTaskPrioritySet(task->handle, PRIORITY_NOT_RUNNING);

  TRACE_record(EVENT_BASIC(TRACE_DEPRIORITIZED), TRACE_TASK_EITHER, task);
}

/// @brief Lowers the priority of all EDF tasks except the selected highest-priority task.
static void deprioritize_other_tasks(const TMB_t *const highest_priority_task) {
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_t *const task = &periodic_tasks[i];
    if (task != highest_priority_task && uxTaskPriorityGet(task->handle) == PRIORITY_RUNNING) {
      deprioritize_task(task);
    }
  }

  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    TMB_t *const task = &aperiodic_tasks[i];
    if (task != highest_priority_task && uxTaskPriorityGet(task->handle) == PRIORITY_RUNNING) {
      deprioritize_task(task);
    }
  }
}

/// @brief Resumes a task
static void release_task(const TMB_t *const task) {
  configASSERT(task != NULL);
  configASSERT(task->handle != NULL);

  TRACE_record(EVENT_BASIC(TRACE_RELEASE), TRACE_TASK_EITHER, task);

  xTaskResumeFromISR(task->handle);
}

/// @brief produce true if currently running task is different from the highest priority task
bool should_update_priorities(const TMB_t *const highest_priority_task) {
  const TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
  TMB_t             *current_task_tmb    = EDF_get_task_by_handle(current_task_handle);

  if (highest_priority_task == NULL) {
    // No EDF tasks want to run.
    // We only need to update if an EDF task is currently running and needs to be stopped.
    return current_task_tmb != NULL;
  }

  // If no EDF task is currently running (idle/system task is running),
  // and an EDF task is ready, we must update priorities to dispatch it.
  if (current_task_tmb == NULL) {
    return true;
  }

  // Prevent context switch between two tasks with the same deadline
  const bool equal_deadlines = (current_task_tmb->absolute_deadline == highest_priority_task->absolute_deadline);
  if (equal_deadlines && !current_task_tmb->is_done) {
    return false;
  }

  // An EDF task wants to run. Only return true if it is not already running?
  return highest_priority_task->handle != current_task_handle;
}

/// @brief Updates priorities of the (potentially) currently running task, as well as the (potentially) new highest
/// priority task.
void update_priorities() {
  TMB_t *const highest_priority_task = EDF_produce_highest_priority_task();
  const bool   should_update         = should_update_priorities(highest_priority_task);
  if (!should_update) {
    return;
  }

  TRACE_record(EVENT_BASIC(TRACE_UPDATING_PRIORITIES), TRACE_TASK_SYSTEM, NULL);

  deprioritize_other_tasks(highest_priority_task);

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
    set_highest_priority(highest_priority_task);
  }
}

/// @brief Loops through all tasks in an array, and checks whether they have exceeded their deadline, and whether they
/// should be released.
void check_deadlines_and_release_times(const TMB_t *const tasks, const size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const TMB_t *const task         = &tasks[i];
    const bool         task_done    = task->is_done;
    const TickType_t   current_tick = xTaskGetTickCountFromISR();

    // Checks if the task has missed its deadline
    const bool deadline_missed = (current_tick > task->absolute_deadline);
    if (!task_done && deadline_missed) {
      deadline_miss(task);
    }

    // Checks if a task should be released
    const bool task_released  = (task->release_time <= current_tick);
    const bool task_suspended = (eTaskGetState(task->handle) == eSuspended);
    if (!task_done && task_released && task_suspended) {
      release_task(task);
    }
  }
}

/// @brief Calculates release time for dropped task
TickType_t calculate_release_time_for_new_task(const TickType_t new_period) {
  const TickType_t H            = compute_hyperperiod(new_period, periodic_tasks, periodic_task_count);
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
void deadline_miss(const TMB_t *const task) {
  TRACE_record(EVENT_BASIC(TRACE_DEADLINE_MISS), TRACE_TASK_EITHER, task);
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
  reschedule_periodic_tasks();

  check_deadlines_and_release_times(periodic_tasks, periodic_task_count);
  check_deadlines_and_release_times(aperiodic_tasks, aperiodic_task_count);

  update_priorities();
}

static void trace_task_switch(TraceEventType_t switch_event) {
  const TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  const unsigned int current_core = portGET_CORE_ID();

  TaskHandle_t idle_tasks[configNUMBER_OF_CORES];
  bool         current_task_is_idle = false;
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    idle_tasks[core] = xTaskGetIdleTaskHandleForCore(core);
    if (current_task == idle_tasks[core])
      current_task_is_idle = true;
  }

  // Keep tracing constrained to the designated EDF execution core in SMP mode.
  // if (current_core != configTICK_CORE) {
  //   return;
  // }

  if (current_task == NULL)
    return;

#if TRACE_WITH_LOGIC_ANALYZER
  update_gpio_pin(idle_task, current_task, 0);
#else
  if (current_task_is_idle) {
    TRACE_record(EVENT_BASIC(switch_event), TRACE_TASK_IDLE, NULL);
    return;
  }

  const TMB_t *const current_task_tmb = EDF_get_task_by_handle(current_task);
  if (current_task_tmb == NULL) {
    TRACE_record(EVENT_BASIC(switch_event), TRACE_TASK_SYSTEM, NULL);
    return;
  }

  TRACE_record(EVENT_BASIC(switch_event), TRACE_TASK_EITHER, current_task_tmb);
#endif
}

/// @brief Tick hook called whenever a task is switched out by the scheduler.
void task_switched_out(void) { trace_task_switch(TRACE_SWITCH_OUT); }

/// @brief Tick hook called whenever a task is switched in by the scheduler.
void task_switched_in(void) { trace_task_switch(TRACE_SWITCH_IN); }

#if TRACE_WITH_LOGIC_ANALYZER
void update_gpio_pin( //
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
