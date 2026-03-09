#include "edf_scheduler.h"

#include "ProjectConfig.h"
#include "admission_control.h"
#include "helpers.h"
#include "pico/time.h"
#include "srp.h" // IWYU pragma: keep

#include <stdint.h>
#include <stdio.h>

TMB_t  periodic_tasks[MAXIMUM_PERIODIC_TASKS];
size_t periodic_task_count = 0;

TMB_t  aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
size_t aperiodic_task_count = 0;

TraceRecord_t trace_buffer[MAX_TRACE_RECORDS];
size_t        trace_count = 0;

// === LOCAL FUNCTION DECLARATIONS ===
void          reschedule_periodic_tasks();
static TMB_t *candidate_highest_priority(TMB_t *tasks, const size_t count);
static void   set_highest_priority(const TMB_t *const task);
static void   deprioritize_task(const TMB_t *const task);
static void   release_task(const TMB_t *const task);
static void   release_tasks_in_array(const TMB_t *const tasks, const size_t count);
void          release_tasks();
bool          should_update_priorities(const TMB_t *const highest_priority_task);
void          update_priorities();
void          vApplicationTickHook(void);
TickType_t    calculate_release_time_for_new_task(const TickType_t new_period);
void          task_switched_out(void);
void          task_switched_in(void);
void          deadline_miss(const TMB_t *const task);

// Trace stuff
void record_trace_event( //
  const TraceEventType_t event,
  TraceTaskType_t        task_type,
  const TMB_t *const     task,
  const uint8_t          resource_id
);
void print_trace_buffer();

// === API FUNCTION DEFINITIONS ===

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

// --- SRP PREEMPTION CHECK ---
#if USE_SRP
  configASSERT(SRP_initialized());
  const unsigned int current_system_ceiling = SRP_get_system_ceiling();
  if (candidate->preemption_level <= current_system_ceiling) {
    const TaskHandle_t current_task     = xTaskGetCurrentTaskHandle();
    TMB_t             *current_task_tmb = EDF_get_task_by_handle(current_task);
    configASSERT(current_task_tmb != NULL);

    return current_task_tmb;
  }
#endif

  return candidate;
}

// TODO: This should accept the TMB instead of the task handle.
/// @brief Performs all necessary logic to inform the scheduler that a task has finished its execution. Note that
/// calling this function causes the calling task to be suspended.
void EDF_mark_task_done(TaskHandle_t task_handle) {
  // This call to ENTER_CRITICAL is necessary both to prevent race conditions, where a task might get preempted in the
  // middle of calling this function, but also because of the calls to vTaskSuspend and vTaskPrioritySet in the middle
  // of it, which would otherwise cause the scheduler to perform a context switch immediately
  taskENTER_CRITICAL();

  configASSERT(task_handle != NULL);
  TMB_t *const task_tmb = EDF_get_task_by_handle(task_handle);
  configASSERT(task_tmb != NULL);

  vTaskSuspend(task_handle); // Suspension is needed so that idle task will run when no tasks are ready
  task_tmb->is_done = true;
  update_priorities();

  record_trace_event(TRACE_EVENT_DONE, TRACE_TASK_EITHER, task_tmb, 0);

  if (xTaskGetTickCount() > task_tmb->absolute_deadline) {
    deadline_miss(task_tmb);
  }

  taskEXIT_CRITICAL();
}

// TODO: Maybe the pxCreatedTask pointer could return a pointer to the TMB instead, since it also includes the handle?
/// @brief Creates a periodic task and initializes all information the EDF scheduler requires to know about it.
/// REQUIRES: xDeadlinePeriodic <= xPeriod must hold
BaseType_t EDF_create_periodic_task(
  TaskFunction_t               pxTaskCode,
  const char *const            pcName,
  const configSTACK_DEPTH_TYPE uxStackDepth,
  const TickType_t             completionTime,
  const TickType_t             xPeriod,
  const TickType_t             xDeadlineRelative,
  TaskHandle_t *const          pxCreatedTask
) {
  if (periodic_task_count >= MAXIMUM_PERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }

  if (!can_admit_periodic_task(completionTime, xPeriod, xDeadlineRelative)) {
    // TODO: for testing: printf("xTaskCreatePeriodic - admission failed\n");
    return errADMISSION_FAILED;
  } else {
    // TODO: for testing: printf("xTaskCreatePeriodic - admission: %s successed\n", pcName);
  }
  configASSERT(xDeadlineRelative <= xPeriod);

  TaskHandle_t task_handle;
  const bool   isSchedulerStarted = xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
  // TODO: priority of task below is a magic number
  // Q: Should the priority below really be "1" (not done - not running?)

  const BaseType_t result = xTaskCreate( //
    pxTaskCode,
    pcName,
    uxStackDepth,
    (void *)completionTime,
    PRIORITY_NOT_RUNNING,
    &task_handle
  );
  // TODO: it might be a bit redundant to pass completion time as a reference and also
  // pass it as a parameter to created task (a single handle to TMB might be sufficient)
  if (result == pdPASS) {
    const size_t index    = periodic_task_count; // Store the current count as the index for the new task
    TMB_t *const new_task = &periodic_tasks[index];
    periodic_task_count++;

    new_task->type    = TASK_PERIODIC;
    new_task->id      = index;
    new_task->handle  = task_handle;
    new_task->is_done = false;

    new_task->completion_time = completionTime;

    new_task->periodic.period            = xPeriod;
    new_task->periodic.relative_deadline = xDeadlineRelative;

    if (!isSchedulerStarted) {
      new_task->release_time         = xTaskGetTickCount();
      new_task->periodic.next_period = xTaskGetTickCount() + xPeriod;
      new_task->absolute_deadline    = xTaskGetTickCount() + xDeadlineRelative;
    } else {
      TickType_t release_time        = calculate_release_time_for_new_task(xPeriod);
      new_task->release_time         = release_time;
      new_task->periodic.next_period = release_time + xPeriod;
      new_task->absolute_deadline    = release_time + xDeadlineRelative;
    }
    vTaskSuspend(task_handle);

    if (pxCreatedTask != NULL) {
      *pxCreatedTask = task_handle;
    }
  } else {
    if (pxCreatedTask != NULL) {
      *pxCreatedTask = NULL;
    }
  }
  return result;
}

// TODO: Implement the xTaskDeleteAperiodic function, which will be responsible for deleting
// aperiodic tasks once they are done executing.  This is necessary to prevent memory leaks, since
// aperiodic tasks are not reused like periodic tasks.
/// @brief Creates an aperiodic task and initializes all information the EDF scheduler requires to know about it.
BaseType_t EDF_create_aperiodic_task(
  TaskFunction_t               pxTaskCode,        // Task function
  const char *const            pcName,            // Task name
  const configSTACK_DEPTH_TYPE uxStackDepth,      // Stack depth
  const TickType_t             completionTime,    // Completion time
  const TickType_t             xReleaseTime,      // Release time
  const TickType_t             xDeadlineRelative, // Relative Deadline
  TaskHandle_t *const          pxCreatedTask      // Task handle
) {
  if (aperiodic_task_count >= MAXIMUM_APERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }

  TaskHandle_t     task_handle;
  const bool       isSchedulerStarted = xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
  const BaseType_t result             = xTaskCreate( //
    pxTaskCode,
    pcName,
    uxStackDepth,
    (void *)completionTime,
    PRIORITY_NOT_RUNNING,
    &task_handle
  );

  if (result == pdPASS) {
    const size_t index    = aperiodic_task_count; // Store the current count as the index for the new task
    TMB_t *const new_task = &aperiodic_tasks[index];
    aperiodic_task_count++;

    new_task->type    = TASK_APERIODIC;
    new_task->id      = index;
    new_task->handle  = task_handle;
    new_task->is_done = false;

    new_task->completion_time   = completionTime;
    new_task->release_time      = xReleaseTime;
    new_task->absolute_deadline = xReleaseTime + xDeadlineRelative;

    vTaskSuspend(task_handle);

    if (pxCreatedTask != NULL) {
      *pxCreatedTask = task_handle;
    }
  } else {
    if (pxCreatedTask != NULL) {
      *pxCreatedTask = NULL;
    }
  }

  return result;
}

/// @brief Task function for periodic tasks. It will run until it has executed for a number of time
/// slices equal to its completion time, at which point it will mark itself as done and suspend
/// itself. Note that the task relies on the scheduler to mark it as not done and resume it when its
/// next period starts.
/// @param pvParameters
void EDF_periodic_task(void *pvParameters) {
  // TODO: Replace with macro, so that the scheduler can be responsible for marking tasks as done
  // and suspending them, instead of the tasks themselves.
  // TODO: This would also mean that the scheduler can be responsible for deleting aperiodic tasks
  // once they are finished executing.
  const BaseType_t xCompletionTime = (BaseType_t)pvParameters;
  const TickType_t previousTick    = xTaskGetTickCount();

  for (;;) {
    execute_for_ticks(xCompletionTime);
    EDF_mark_task_done(xTaskGetCurrentTaskHandle());
  }
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
  /* Start the tasks and timer running. */
  printf("Starting scheduler.\n");

  vApplicationTickHook();

  vTaskStartScheduler();
}

// === LOCAL FUNCTION DEFINITIONS ===

/// @brief Re-schedules all periodic tasks whose periods have elapsed, and should run again
void reschedule_periodic_tasks() {
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_t *const task = &periodic_tasks[i];
    if (task->is_done) {
      const TickType_t current_tick = xTaskGetTickCount();
      if (current_tick >= task->periodic.next_period) {
        task->absolute_deadline    = task->periodic.next_period + task->periodic.relative_deadline;
        task->periodic.next_period = task->periodic.next_period + task->periodic.period;
        task->is_done              = false;
        xTaskResumeFromISR(task->handle);

        // TODO: resource_id should be UINT8_MAX when not used
        record_trace_event(TRACE_EVENT_RESCHEDULED, TRACE_TASK_PERIODIC, task, 0);
      }
    }
  }
}

/// @brief Helper function to find the pending task with the nearest deadline
static TMB_t *candidate_highest_priority(TMB_t *tasks, const size_t count) {
  const TickType_t current_tick      = xTaskGetTickCount();
  TMB_t           *candidate         = NULL;
  TickType_t       earliest_deadline = portMAX_DELAY;

  for (size_t i = 0; i < count; ++i) {
    TMB_t *task = &tasks[i];
    if (!task->is_done && current_tick >= task->release_time && task->absolute_deadline < earliest_deadline) {
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

  record_trace_event(TRACE_EVENT_PRIORITY_SET, TRACE_TASK_EITHER, task, 0);
}

/// @brief Lowers the priority of a task to the lowest possible/minimum, which should prevent it from running.
static void deprioritize_task(const TMB_t *const task) {
  configASSERT(task != NULL);
  configASSERT(task->handle != NULL);

  vTaskPrioritySet(task->handle, PRIORITY_NOT_RUNNING);

  record_trace_event(TRACE_EVENT_DEPRIORITIZED, TRACE_TASK_EITHER, task, 0);
}

/// @brief Resumes a task
static void release_task(const TMB_t *const task) {
  configASSERT(task != NULL);
  configASSERT(task->handle != NULL);

  record_trace_event(TRACE_EVENT_RELEASE, TRACE_TASK_EITHER, task, 0);

  xTaskResumeFromISR(task->handle);
}

/// @brief Loops through the provided array and resumes all tasks which have reached/surpassed their release time
static void release_tasks_in_array(const TMB_t *const tasks, const size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const TMB_t *const task           = &tasks[i];
    const bool         task_done      = task->is_done;
    const bool         task_released  = task->release_time <= xTaskGetTickCount();
    const bool         task_suspended = eTaskGetState(task->handle) == eSuspended;
    if (!task_done && task_released && task_suspended) {
      release_task(task);
    }
  }
}

/// @brief Calls release_tasks_in_array on both the periodic and aperiodic tasks
void release_tasks() {
  release_tasks_in_array(periodic_tasks, periodic_task_count);
  release_tasks_in_array(aperiodic_tasks, aperiodic_task_count);
}

/// @brief produce true if currently running task is different from the highest priority task
bool should_update_priorities(const TMB_t *const highest_priority_task) {
  const TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

  // If there are no schedulable tasks, then we should be running the idle task. In that case, we only want to update
  // priorities if we're not already running the idle task (i.e. if current_task is not the idle task handle).
  if (highest_priority_task == NULL) {
    return current_task != xTaskGetIdleTaskHandle();
  }

  return highest_priority_task->handle != current_task;
}

/// @brief Updates priorities of the (potentially) currently running task, as well as the (potentially) new highest
/// priority task.
void update_priorities() {
  const TMB_t *const highest_priority_task = EDF_produce_highest_priority_task();
  const bool         should_update         = should_update_priorities(highest_priority_task);
  if (!should_update) {
    return;
  }

  record_trace_event(TRACE_EVENT_UPDATING_PRIORITIES, TRACE_TASK_SYSTEM, highest_priority_task, 0);

  TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
  TMB_t       *current_task        = EDF_get_task_by_handle(current_task_handle);
  // configASSERT(current_task != NULL);

  if (current_task != NULL) {
    const UBaseType_t task_priority = uxTaskPriorityGet(current_task_handle);
    if (task_priority == PRIORITY_RUNNING) {
      deprioritize_task(current_task);
    }
  }

  // If new_highest_priority_task is NULL, that means there are no schedulable tasks and we should be running the idle
  // task. In that case, we shouldn't set a new highest priority task, and the FreeRTOS scheduler should instead elect
  // to run the Idle task.
  if (highest_priority_task != NULL) {
    set_highest_priority(highest_priority_task);
  }
}

/// @brief Tick hook to ensure the EDF extension's logic is run before the FreeRTOS scheduler every tick
void vApplicationTickHook(void) {
  reschedule_periodic_tasks();
  release_tasks();
  update_priorities();
}

/// @brief Calculates release time for dropped task
TickType_t calculate_release_time_for_new_task(const TickType_t new_period) {
  const TickType_t H = compute_hyperperiod(new_period);
  // Hypothesis: value of xNow doesn't change during duration of function body's
  // execution if function is only called in context of tick hook
  const TickType_t xNow = xTaskGetTickCount();
  // NB: Theoretically, we shouldn't hit this code block
  if (xNow == 0)
    return 0;
  const TickType_t remainder = xNow % H;
  if (remainder == 0) {
    return xNow;
  } else {
    return xNow + (H - remainder);
  }
}

/// @brief Tick hook called whenever a task is switched out by the scheduler.
void task_switched_out(void) {
  const TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  const TaskHandle_t idle_task    = xTaskGetIdleTaskHandle();

  if (current_task == NULL)
    return;

#if TRACE_WITH_LOGIC_ANALYZER
  if (current_task == idle_task) {
    gpio_put(mainGPIO_LED_TASK_4, 0);
  } else if (current_task == periodic_tasks[0].tmb.handle) {
    gpio_put(mainGPIO_LED_TASK_1, 0);
  } else if (current_task == periodic_tasks[1].tmb.handle) {
    gpio_put(mainGPIO_LED_TASK_2, 0);
  } else if (current_task == periodic_tasks[2].tmb.handle) {
    gpio_put(mainGPIO_LED_TASK_3, 0);
  } else {
    gpio_put(mainGPIO_LED_TASK_5, 0);
  }
#else
  if (current_task == idle_task) {
    record_trace_event(TRACE_EVENT_SWITCH_OUT, TRACE_TASK_IDLE, NULL, 0);
    return;
  }

  const TMB_t *const current_task_tmb = EDF_get_task_by_handle(current_task);
  if (current_task_tmb == NULL) {
    // This should never happen, but just in case
    record_trace_event(TRACE_EVENT_SWITCH_OUT, TRACE_TASK_SYSTEM, NULL, 0);
    return;
  }

  record_trace_event(TRACE_EVENT_SWITCH_OUT, TRACE_TASK_EITHER, current_task_tmb, 0);
#endif
}

/// @brief Tick hook called whenever a task is switched in by the scheduler.
void task_switched_in(void) {
  const TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  const TaskHandle_t idle_task    = xTaskGetIdleTaskHandle();

  // Can this ever happen?
  if (current_task == NULL)
    return;

#if TRACE_WITH_LOGIC_ANALYZER
  if (current_task == idle_task) {
    gpio_put(mainGPIO_LED_TASK_4, 1);
  } else if (current_task == periodic_tasks[0].tmb.handle) {
    gpio_put(mainGPIO_LED_TASK_1, 1);
  } else if (current_task == periodic_tasks[1].tmb.handle) {
    gpio_put(mainGPIO_LED_TASK_2, 1);
  } else if (current_task == periodic_tasks[2].tmb.handle) {
    gpio_put(mainGPIO_LED_TASK_3, 1);
  } else {
    gpio_put(mainGPIO_LED_TASK_5, 1);
  }
#else
  if (current_task == idle_task) {
    record_trace_event(TRACE_EVENT_SWITCH_IN, TRACE_TASK_IDLE, NULL, 0);
    return;
  }

  const TMB_t *const current_task_tmb = EDF_get_task_by_handle(current_task);
  if (current_task_tmb == NULL) {
    // This should never happen, but just in case
    record_trace_event(TRACE_EVENT_SWITCH_IN, TRACE_TASK_SYSTEM, NULL, 0);
    return;
  }

  record_trace_event(TRACE_EVENT_SWITCH_IN, TRACE_TASK_EITHER, current_task_tmb, 0);
#endif
}

/// @brief Logic for whatever should happen when a deadline is missed
// TODO: This needs to be re-implemented
void deadline_miss(const TMB_t *const task) {
  // configASSERT(task != NULL);
  // vTaskSuspendAll(); // Freeze the scheduler to prevent being preempted in the middle of printing the error message
  // and rebooting

  // record_trace_event(TRACE_EVENT_DEADLINE_MISS, (task->type == TASK_PERIODIC) ? TRACE_TASK_PERIODIC :
  // TRACE_TASK_APERIODIC, task, 0);

  // printf(
  //   "Time: %lu FATAL: Task %d missed its deadline of %lu ticks!\n", xTaskGetTickCount(), task->id,
  //   task->absolute_deadline
  // );

  // print_trace_buffer();

  // vTaskDelay(pdMS_TO_TICKS(5000)); // Delay to ensure all trace events are printed

  // configASSERT(false);

  // watchdog_enable(1, 1); // Reboot the system immediately
}

// TODO: This function should maybe differ when SRP is enabled vs when it is not, since the trace event structure is a
// bit different for SRP vs EDF. For now, just include all SRP-related fields in the trace event, but they will be set
// to 0 when SRP is not enabled.
/// @brief Records a trace, so debugging is simpler even without a logic analyzer
void record_trace_event( //
  const TraceEventType_t event,
  TraceTaskType_t        task_type,
  const TMB_t *const     task,
  const uint8_t          resource_id
) {
  if (task_type == TRACE_TASK_EITHER) {
    configASSERT(task != NULL);
    task_type = (task->type == TASK_PERIODIC) ? TRACE_TASK_PERIODIC : TRACE_TASK_APERIODIC;
  }

  taskENTER_CRITICAL();
  if (trace_count < MAX_TRACE_RECORDS) {
    trace_buffer[trace_count].FreeRTOS_tick = xTaskGetTickCount();
    trace_buffer[trace_count].time          = get_absolute_time();
    trace_buffer[trace_count].event_type    = event;
    trace_buffer[trace_count].resource_id   = resource_id;
    trace_buffer[trace_count].task_type     = task_type;

    if (task != NULL) {
      trace_buffer[trace_count].task_id  = task->id;
      trace_buffer[trace_count].deadline = task->absolute_deadline;
      if (task->handle != NULL) {
        trace_buffer[trace_count].priority   = uxTaskPriorityGet(task->handle);
        trace_buffer[trace_count].task_state = eTaskGetState(task->handle);
      } else {
        trace_buffer[trace_count].priority   = portMAX_DELAY; // Set priority to a default value (e.g., max)
        trace_buffer[trace_count].task_state = eInvalid;
      }
    } else {
      trace_buffer[trace_count].task_id    = UINT8_MAX;
      trace_buffer[trace_count].deadline   = portMAX_DELAY;
      trace_buffer[trace_count].priority   = portMAX_DELAY;
      trace_buffer[trace_count].task_state = eInvalid;
    }

#if USE_SRP
    trace_buffer[trace_count].system_ceiling = SRP_get_system_ceiling(); // Grab current ceiling dynamically
    if (task != NULL) {
      trace_buffer[trace_count].preempt_level = task->preemption_level;
    } else {
      trace_buffer[trace_count].preempt_level = 0; // For idle task or system events, we can set preemption level to 0
    }
#else
    trace_buffer[trace_count].system_ceiling = 0; // Not used when SRP is disabled
    trace_buffer[trace_count].preempt_level  = 0; // Not used when SRP is disabled
#endif

    trace_count++;
  }
  taskEXIT_CRITICAL();
}

/// @brief Prints all recorded traces to the host computer
void print_trace_buffer() {
  printf("\n--- TEST COMPLETE ---\n");
  printf("Traces captured: %u\n", trace_count);
  printf("TIMESTAMP,EVENT,ABS_TIME,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,CEILING,PREEMPT_LVL,DEADLINE\n");

  // clang-format off
  for (size_t i = 0; i < trace_count; i++) {
    const TraceRecord_t *const r = &trace_buffer[i];
    printf(
      "%u,%d,%llu,%d,%u,%u,%d,%u,%u,%u,%u\n",
      r->FreeRTOS_tick,
      r->event_type,
      to_us_since_boot(r->time),
      r->task_type,
      r->task_id,
      (unsigned int)r->priority,
      (int)r->task_state,
      r->resource_id,
      r->system_ceiling,
      r->preempt_level,
      r->deadline
    );
  }
  // clang-format on

  printf("--- END OF TRACE ---\n");
}

// TODO: Add another tick hook for suspended tasks, or for when tasks are deprioritized. When tasks are marked as done,
// the scheduler will choose another task to run, and relies on the default priority of the tasks in that case.
// TODO: An alternative to the above would be to move away from only raising the priority of the next task, and instead
// give tasks a priority proportional to their index in a sorted list of deadlines.
