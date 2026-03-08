#include "edf_scheduler.h"

#include "ProjectConfig.h"
#include "admission_control.h"
#include "helpers.h"
#include "pico/time.h"
#include "srp.h"

#include <stdint.h>
#include <stdio.h>

TMB_t  periodic_tasks[MAXIMUM_PERIODIC_TASKS];
size_t periodic_task_count = 0;

TMB_t  aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
size_t aperiodic_task_count = 0;

TraceRecord_t trace_buffer[MAX_TRACE_RECORDS];
size_t        trace_count = 0;

void setSchedulable();

void       deprioritizeAllTasks();
void       releaseTasks();
TickType_t calculate_release_time_for_dropped_task(TickType_t new_period);

bool should_update_priorities(TMB_t **task_highest_priority);
void updatePriorities();

void setSchedulable() {
  // Iterate through all periodic tasks and set their deadlines
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_t *task = &periodic_tasks[i];
    if (task->is_done) {
      TickType_t current_tick = xTaskGetTickCount();
      if (current_tick >= task->periodic.next_period) {
        task->absolute_deadline    = task->periodic.next_period + task->periodic.relative_deadline;
        task->periodic.next_period = task->periodic.next_period + task->periodic.period;
        task->is_done              = false;
        record_trace_event(TRACE_EVENT_RELEASE, TRACE_TASK_PERIODIC, task, 0);
      }
    }
  }
}

// Helper function to find the pending task with the nearest deadline
static TMB_t *get_highest_priority_candidate(TMB_t *tasks, size_t count) {
  TMB_t     *candidate         = NULL;
  TickType_t earliest_deadline = portMAX_DELAY;
  TickType_t current_tick      = xTaskGetTickCount();

  for (size_t i = 0; i < count; ++i) {
    TMB_t *task = &tasks[i];
    if (!task->is_done && current_tick >= task->release_time && task->absolute_deadline < earliest_deadline) {
      candidate         = task;
      earliest_deadline = task->absolute_deadline;
    }
  }

  return candidate;
}

/// @brief Return task handle of highest priority task in TMB arrays. Return NULL if none
TMB_t *produce_highest_priority_task() {
  TMB_t *periodic_candidate  = get_highest_priority_candidate(periodic_tasks, periodic_task_count);
  TMB_t *aperiodic_candidate = get_highest_priority_candidate(aperiodic_tasks, aperiodic_task_count);

  // // Early return if there are no tasks available
  if (periodic_candidate == NULL && aperiodic_candidate == NULL) {
    return NULL;
  }

  TickType_t periodic_deadline = (periodic_candidate != NULL) ? periodic_candidate->absolute_deadline : portMAX_DELAY;
  TickType_t aperiodic_deadline =
    (aperiodic_candidate != NULL) ? aperiodic_candidate->absolute_deadline : portMAX_DELAY;
  TMB_t *candidate = (periodic_deadline < aperiodic_deadline) ? periodic_candidate : aperiodic_candidate;

// --- SRP PREEMPTION CHECK ---
#if USE_SRP
  configASSERT(srp_is_initialized());
  unsigned int current_system_ceiling = get_srp_system_ceiling();
  if (candidate->preemption_level <= current_system_ceiling) {
    TaskHandle_t current_task     = xTaskGetCurrentTaskHandle();
    TMB_t       *current_task_tmb = get_task_by_handle(current_task);
    configASSERT(current_task_tmb != NULL);

    return current_task_tmb;
  }
#endif

  return candidate;
}

static void set_highest_priority(TMB_t *task) {
  configASSERT(task != NULL);
  configASSERT(task->handle != NULL);
  configASSERT(!task->is_done); // We should never be prioritizing a task that's already done
  vTaskPrioritySet(task->handle, PRIORITY_RUNNING);
  record_trace_event(
    TRACE_EVENT_PRIORITY_SET, (task->type == TASK_PERIODIC) ? TRACE_TASK_PERIODIC : TRACE_TASK_APERIODIC, task, 0
  );
}

static void deprioritize_task(TMB_t *task) {
  configASSERT(task != NULL);
  configASSERT(task->handle != NULL);
  configASSERT(!task->is_done); // We should never be deprioritizing a task that's already done
  // configASSERT(
  //   uxTaskPriorityGet(task->handle) != PRIORITY_NOT_RUNNING
  // ); // We should never be deprioritizing a task that's already not running
  vTaskPrioritySet(task->handle, PRIORITY_NOT_RUNNING);
  record_trace_event(
    TRACE_EVENT_DEPRIORITIZED, (task->type == TASK_PERIODIC) ? TRACE_TASK_PERIODIC : TRACE_TASK_APERIODIC, task, 0
  );
  // vTaskSuspend(task->handle);
}

static void release_tasks_in_array(TMB_t *tasks, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    TMB_t *task = &tasks[i];
    if (!task->is_done && task->release_time <= xTaskGetTickCount() && eTaskGetState(task->handle) == eSuspended) {
      xTaskResumeFromISR(task->handle);

      TraceTaskType_t trace_task_type = (task->type == TASK_PERIODIC) ? TRACE_TASK_PERIODIC : TRACE_TASK_APERIODIC;
      record_trace_event(TRACE_EVENT_RELEASE, trace_task_type, task, 0);
    }
  }
}
void releaseTasks() {
  release_tasks_in_array(periodic_tasks, periodic_task_count);
  release_tasks_in_array(aperiodic_tasks, aperiodic_task_count);
}

/// @brief produce true if currently running task is different from the highest priority task
bool should_update_priorities(TMB_t **task_highest_priority) {
  *task_highest_priority    = produce_highest_priority_task();
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

  // If there are no schedulable tasks, then we should be running the idle task. In that case, we only want to update
  // priorities if we're not already running the idle task (i.e. if current_task is not the idle task handle).
  if (*task_highest_priority == NULL) {
    return current_task != xTaskGetIdleTaskHandle();
  }

  return (*task_highest_priority)->handle != current_task;
}

void updatePriorities() {
  TMB_t *new_highest_priority_task = NULL;
  bool   should_update             = should_update_priorities(&new_highest_priority_task);
  if (!should_update) {
    return;
  }

  const size_t new_highest_priority_task_marker =
    (new_highest_priority_task != NULL) ? new_highest_priority_task->id : UINT8_MAX;
  // record_trace_event(TRACE_EVENT_UPDATING_PRIORITIES, TRACE_TASK_SYSTEM, new_highest_priority_task, 0);
  record_trace_event(
    TRACE_EVENT_UPDATING_PRIORITIES, TRACE_TASK_SYSTEM, new_highest_priority_task, new_highest_priority_task_marker
  );

  TaskHandle_t current_task     = xTaskGetCurrentTaskHandle();
  TMB_t       *current_task_tmb = get_task_by_handle(current_task);
  // configASSERT(current_task_tmb != NULL);

  if (current_task_tmb != NULL) {
    // We only want to deprioritize the current task if it's not the idle task, since the idle task is a special case
    // where it should always be ready to run when there are no other tasks.
    if (current_task != xTaskGetIdleTaskHandle()) {
      // configASSERT(
      //   uxTaskPriorityGet(current_task) == PRIORITY_RUNNING
      // ); // We should only be deprioritizing a task that's currently running
      deprioritize_task(current_task_tmb);
    }
  }

  // If new_highest_priority_task is NULL, that means there are no schedulable tasks and we should be running the idle
  // task. In that case, we can just deprioritize the current task and let the scheduler switch to the idle task.
  if (new_highest_priority_task != NULL) {
    set_highest_priority(new_highest_priority_task);
  }
}

void vApplicationTickHook(void) {
  setSchedulable();
  releaseTasks();
  updatePriorities();
}

// TODO: This should accept the TMB instead of the task handle.
void taskPeriodicDone(TaskHandle_t task_handle) {
  taskENTER_CRITICAL();

  configASSERT(task_handle != NULL);
  TMB_t *task_tmb = get_task_by_handle(task_handle);
  configASSERT(task_tmb != NULL);

  TraceTaskType_t trace_task_type = (task_tmb->type == TASK_PERIODIC) ? TRACE_TASK_PERIODIC : TRACE_TASK_APERIODIC;
  record_trace_event(TRACE_EVENT_DONE, trace_task_type, task_tmb, 0);

  task_tmb->is_done = true;

  if (xTaskGetTickCount() > task_tmb->absolute_deadline) {
    deadline_miss(task_tmb);
  }

  taskEXIT_CRITICAL();
}

/// @brief calculates release time for dropped task
TickType_t calculate_release_time_for_dropped_task(TickType_t new_period) {
  TickType_t H = compute_hyperperiod(new_period);
  // Hypothesis: value of xNow doesn't change during duration of function body's
  // execution if function is only called in context of tick hook
  TickType_t xNow = xTaskGetTickCount();
  // NB: Theoretically, we shouldn't hit this code block
  if (xNow == 0)
    return 0;
  TickType_t remainder = xNow % H;
  if (remainder == 0) {
    return xNow;
  } else {
    return xNow + (H - remainder);
  }
}

// REQUIRES: xDeadlinePeriodic <= xPeriod must hold
// TODO: Maybe the pxCreatedTask pointer could return a pointer to the TMB instead, since it also includes the handle?
BaseType_t xTaskCreatePeriodic(
  TaskFunction_t               pxTaskCode,
  const char *const            pcName,
  const configSTACK_DEPTH_TYPE uxStackDepth,
  TickType_t                   completionTime,
  TickType_t                   xPeriod,
  TickType_t                   xDeadlineRelative,
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
  // TODO: priority of task below is a magic number
  // Q: Should the priority below really be "1" (not done - not running?)

  bool       isSchedulerStarted = xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
  BaseType_t result             = xTaskCreate( //
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
    TMB_t       *new_task = &periodic_tasks[index];
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
      record_trace_event(TRACE_EVENT_RELEASE, TRACE_TASK_PERIODIC, new_task, 0);
    } else {
      TickType_t release_time        = calculate_release_time_for_dropped_task(xPeriod);
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
BaseType_t xTaskCreateAperiodic(
  TaskFunction_t               pxTaskCode,        // Task function
  const char *const            pcName,            // Task name
  const configSTACK_DEPTH_TYPE uxStackDepth,      // Stack depth
  TickType_t                   completionTime,    // Completion time
  TickType_t                   xReleaseTime,      // Release time
  TickType_t                   xDeadlineRelative, // Relative Deadline
  TaskHandle_t *const          pxCreatedTask      // Task handle
) {
  if (aperiodic_task_count >= MAXIMUM_APERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }

  TaskHandle_t task_handle;
  bool         isSchedulerStarted = xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
  BaseType_t   result             = xTaskCreate( //
    pxTaskCode,
    pcName,
    uxStackDepth,
    (void *)completionTime,
    PRIORITY_NOT_RUNNING,
    &task_handle
  );

  if (result == pdPASS) {
    const size_t index    = aperiodic_task_count; // Store the current count as the index for the new task
    TMB_t       *new_task = &aperiodic_tasks[index];
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

// TODO: This function should maybe differ when SRP is enabled vs when it is not, since the trace event structure is a
// bit different for SRP vs EDF. For now, just include all SRP-related fields in the trace event, but they will be set
// to 0 when SRP is not enabled.
void record_trace_event(TraceEventType_t event, TraceTaskType_t task_type, TMB_t *task, uint8_t resource_id) {
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
        trace_buffer[trace_count].priority = uxTaskPriorityGet(task->handle);
      } else {
        trace_buffer[trace_count].priority = portMAX_DELAY; // Set priority to a default value (e.g., max)
      }
    } else {
      trace_buffer[trace_count].task_id  = UINT8_MAX;
      trace_buffer[trace_count].deadline = portMAX_DELAY;
      trace_buffer[trace_count].priority = portMAX_DELAY;
    }

#if USE_SRP
    trace_buffer[trace_count].system_ceiling = get_srp_system_ceiling(); // Grab current ceiling dynamically
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

void task_switched_out(void) {
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  TaskHandle_t idle_task    = xTaskGetIdleTaskHandle();

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

  TMB_t *current_task_tmb = get_task_by_handle(current_task);
  if (current_task_tmb == NULL) {
    // This should never happen, but just in case
    record_trace_event(TRACE_EVENT_SWITCH_OUT, TRACE_TASK_SYSTEM, NULL, 0);
    return;
  }

  TraceTaskType_t trace_task_type =
    (current_task_tmb->type == TASK_PERIODIC) ? TRACE_TASK_PERIODIC : TRACE_TASK_APERIODIC;
  record_trace_event(TRACE_EVENT_SWITCH_OUT, trace_task_type, current_task_tmb, 0);
#endif
}

void task_switched_in(void) {
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  TaskHandle_t idle_task    = xTaskGetIdleTaskHandle();

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

  TMB_t *current_task_tmb = get_task_by_handle(current_task);
  if (current_task_tmb == NULL) {
    // This should never happen, but just in case
    record_trace_event(TRACE_EVENT_SWITCH_IN, TRACE_TASK_SYSTEM, NULL, 0);
    return;
  }

  TraceTaskType_t trace_task_type =
    (current_task_tmb->type == TASK_PERIODIC) ? TRACE_TASK_PERIODIC : TRACE_TASK_APERIODIC;
  record_trace_event(TRACE_EVENT_SWITCH_IN, trace_task_type, current_task_tmb, 0);
#endif
}

/// @brief  Task function for periodic tasks. It will run until it has executed for a number of time
/// slices equal to its completion time, at which point it will mark itself as done and suspend
/// itself. Note that the task relies on the scheduler to mark it as not done and resume it when its
/// next period starts.
/// @param pvParameters
void vPeriodicTask(void *pvParameters) {
  // TODO: Replace with macro, so that the scheduler can be responsible for marking tasks as done
  // and suspending them, instead of the tasks themselves.
  // TODO: This would also mean that the scheduler can be responsible for deleting aperiodic tasks
  // once they are finished executing.
  const BaseType_t xCompletionTime = (BaseType_t)pvParameters;
  TickType_t       previousTick    = xTaskGetTickCount();

  BaseType_t xTimeSlicesExecutedThusFar = 0;

  for (;;) {
    execute_for_ticks(xCompletionTime);
    taskPeriodicDone(xTaskGetCurrentTaskHandle());
    vTaskSuspend(NULL); // TODO: Do we really need to suspend at the end here?
  }
}

// TODO: Some way of providing the task type to speed up the function, if only looking for periodic tasks or only
// looking for aperiodic tasks?
TMB_t *get_task_by_handle(TaskHandle_t handle) {
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

void print_trace_buffer() {
  printf("\n--- TEST COMPLETE ---\n");
  printf("Traces captured: %u\n", trace_count);
  printf("TIMESTAMP,EVENT,ABS_TIME,TASK_TYPE,TASK_ID,PRIORITY,RESOURCE,CEILING,PREEMPT_LVL,DEADLINE\n");

  // clang-format off
  for (size_t i = 0; i < trace_count; i++) {
    TraceRecord_t *r = &trace_buffer[i];
    printf(
      "%u,%d,%llu,%d,%u,%u,%u,%u,%u,%u\n",
      r->FreeRTOS_tick,
      r->event_type,
      to_us_since_boot(r->time),
      r->task_type,
      r->task_id,
      (unsigned int)r->priority,
      r->resource_id,
      r->system_ceiling,
      r->preempt_level,
      r->deadline
    );
  }
  // clang-format on

  printf("--- END OF TRACE ---\n");
}

void deadline_miss(TMB_t *task) {
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

void EDF_scheduler_start() {
  /* Start the tasks and timer running. */
  printf("Starting scheduler.\n");

  // Set the highest priority to the task with the earliest deadline at the moment of scheduler start
  TMB_t *initial_highest_priority_task = produce_highest_priority_task();
  if (initial_highest_priority_task != NULL) {
    set_highest_priority(initial_highest_priority_task);
  }

  vTaskStartScheduler();
}


// TODO: Add another tick hook for suspended tasks, or for when tasks are deprioritized. When tasks are marked as done,
// the scheduler will choose another task to run, and relies on the default priority of the tasks in that case.
// TODO: An alternative to the above would be to move away from only raising the priority of the next task, and instead
// give tasks a priority proportional to their index in a sorted list of deadlines.
