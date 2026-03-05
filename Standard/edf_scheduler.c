#include "edf_scheduler.h"

#include "ProjectConfig.h"
#include "admission_control.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "helpers.h"
#include "main_blinky.h"
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

void       setHighestPriority();
void       deprioritizeAllTasks();
void       resumeAllTasks();
TickType_t calculate_release_time_for_dropped_task(TickType_t new_period);

bool should_update_priorities();
void updatePriorities();

void setSchedulable() {
  // Iterate through all periodic tasks and set their deadlines
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_t *task = &periodic_tasks[i];
    if (task->is_done) {
      TickType_t current_tick = xTaskGetTickCount();
      // TODO: This should be "if the last *period* has passed", not just the last deadline.
      // If the last deadline has passed, set a new deadline
      if (current_tick >= task->periodic.next_period) {
        task->absolute_deadline    = task->periodic.next_period + task->periodic.relative_deadline;
        task->periodic.next_period = task->periodic.next_period + task->periodic.period;
        task->is_done              = false;
        record_trace_event(TRACE_EVENT_RELEASE, TRACE_TASK_PERIODIC, i + 1, 0, 0, task->absolute_deadline);
        // vTaskResume(task->tmb.handle); // Shouldn't matter wether the task is already running
      }
    }
  }
}

// Helper function to find the pending task with the nearest deadline
static TMB_t *get_highest_priority_candidate(TMB_t *tasks, size_t count) {
  TMB_t     *candidate         = NULL;
  TickType_t earliest_deadline = (TickType_t)(UINT32_MAX);

  for (size_t i = 0; i < count; ++i) {
    TMB_t *task = &tasks[i]; // Point directly to the array element

    if (!task->is_done && task->absolute_deadline < earliest_deadline) {
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

  TickType_t periodic_deadline =
    (periodic_candidate != NULL) ? periodic_candidate->absolute_deadline : (TickType_t)(UINT32_MAX);
  TickType_t aperiodic_deadline =
    (aperiodic_candidate != NULL) ? aperiodic_candidate->absolute_deadline : (TickType_t)(UINT32_MAX);
  TMB_t *candidate = (periodic_deadline < aperiodic_deadline) ? periodic_candidate : aperiodic_candidate;

// --- SRP PREEMPTION CHECK ---
#if USE_SRP
  configASSERT(srp_is_initialized());
  volatile unsigned int current_system_ceiling = get_srp_system_ceiling();
  if (candidate->preemption_level <= current_system_ceiling) {
    TaskHandle_t current_task     = xTaskGetCurrentTaskHandle();
    TMB_t       *current_task_tmb = get_task_by_handle(current_task);
    return current_task_tmb; // TODO: This should return the TMB_t of the currently running task, not just NULL.
  }
#endif

  return candidate;
}

/// @brief Iterates through tasks and set the highest priority to the task with the nearest deadline
void setHighestPriority() {
  TMB_t *earliest_task = produce_highest_priority_task();

  if (earliest_task != NULL) {
    // Set the priority of the task with the nearest deadline to the highest priority
    vTaskPrioritySet(earliest_task->handle, PRIORITY_RUNNING);
  }
}

static void deprioritize_tasks_in_array(TMB_t *tasks, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    TMB_t *task = &tasks[i];
    if (!task->is_done) {
      vTaskPrioritySet(task->handle, PRIORITY_NOT_RUNNING);
    }
  }
}
void deprioritizeAllTasks() {
  deprioritize_tasks_in_array(periodic_tasks, periodic_task_count);
  deprioritize_tasks_in_array(aperiodic_tasks, aperiodic_task_count);
}

static void resume_tasks_in_array(TMB_t *tasks, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    TMB_t *task = &tasks[i];
    if (!task->is_done && task->release_time <= xTaskGetTickCount()) {
      vTaskResume(task->handle);
      record_trace_event(
        TRACE_EVENT_RELEASE, (task->type == TASK_PERIODIC) ? TRACE_TASK_PERIODIC : TRACE_TASK_APERIODIC, i + 1, 0, 0,
        task->absolute_deadline
      );
    }
  }
}
void resumeAllTasks() {
  resume_tasks_in_array(periodic_tasks, periodic_task_count);
  resume_tasks_in_array(aperiodic_tasks, aperiodic_task_count);
}

void taskPeriodicDone(TaskHandle_t task_handle) {
  taskENTER_CRITICAL();

  // Mark the task as done
  // TODO: Some way of providing the task type to speed up the function?
  TMB_t *task_tmb = get_task_by_handle(task_handle);
  configASSERT(task_tmb != NULL);

  task_tmb->is_done = true;

  if (xTaskGetTickCount() > task_tmb->absolute_deadline) {
    printf("Resetting system...\n");
    // Allow the UART to flush the message before resetting
    busy_wait_us_32(5000);
    // reboot - wait for reboot in low power mode
    watchdog_reboot(0, 0, 0);
    while (1) {
      __wfi();
    };
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
// TODO: The pvParameters argument should be renamed to xCompletionTime.
BaseType_t xTaskCreatePeriodic(
  TaskFunction_t               pxTaskCode,        // Task function
  const char *const            pcName,            // Task name
  const configSTACK_DEPTH_TYPE uxStackDepth,      // Stack depth
  void *const                  pvParameters,      // Completion time
  TickType_t                   xPeriod,           // Period
  TickType_t                   xDeadlineRelative, // Relative Deadline
  TaskHandle_t *const          pxCreatedTask      // Task handle
) {
  if (periodic_task_count >= MAXIMUM_PERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }

  if (!can_admit_periodic_task((TickType_t)pvParameters, xPeriod, xDeadlineRelative)) {
    // TODO: for testing: printf("xTaskCreatePeriodic - admission failed\n");
    return errADMISSION_FAILED;
  } else {
    // TODO: for testing: printf("xTaskCreatePeriodic - admission: %s successed\n", pcName);
  }
  configASSERT(xDeadlineRelative <= xPeriod);

  TaskHandle_t task_handle;
  // TODO: priority of task below is a magic number
  // Q: Should the priority below really be "1" (not done - not running?)

  bool       isSchedulerStarted = xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED;
  BaseType_t result = xTaskCreate(pxTaskCode, pcName, uxStackDepth, pvParameters, 2, &task_handle);
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

    new_task->completion_time = (TickType_t)pvParameters;

    new_task->periodic.period            = xPeriod;
    new_task->periodic.relative_deadline = xDeadlineRelative;

    if (isSchedulerStarted) {
      new_task->release_time         = xTaskGetTickCount();
      new_task->periodic.next_period = xTaskGetTickCount() + xPeriod;
      new_task->absolute_deadline    = xTaskGetTickCount() + xDeadlineRelative;
    } else {
      TickType_t release_time        = calculate_release_time_for_dropped_task(xPeriod);
      new_task->release_time         = release_time;
      new_task->periodic.next_period = release_time + xPeriod;
      new_task->absolute_deadline    = release_time + xDeadlineRelative;
      vTaskSuspend(task_handle);
    }

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
  void *const                  pvParameters,      // Completion time
  TickType_t                   xReleaseTime,      // Release time
  TickType_t                   xDeadlineRelative, // Relative Deadline
  TaskHandle_t *const          pxCreatedTask      // Task handle
) {
  if (aperiodic_task_count >= MAXIMUM_APERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }

  TaskHandle_t task_handle;
  bool         isSchedulerStarted = xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
  BaseType_t   result = xTaskCreate(pxTaskCode, pcName, uxStackDepth, pvParameters, PRIORITY_NOT_RUNNING, &task_handle);

  if (result == pdPASS) {
    const size_t index    = aperiodic_task_count; // Store the current count as the index for the new task
    TMB_t       *new_task = &aperiodic_tasks[index];
    aperiodic_task_count++;

    new_task->type    = TASK_APERIODIC;
    new_task->id      = index;
    new_task->handle  = task_handle;
    new_task->is_done = false;

    new_task->completion_time   = (TickType_t)pvParameters;
    new_task->release_time      = xReleaseTime;
    new_task->absolute_deadline = xReleaseTime + xDeadlineRelative;

    if (!isSchedulerStarted) {
      // Suspend the task so FreeRTOS doesn't instantly run it before its release time
      vTaskSuspend(task_handle);
    }

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

/// @brief produce true if currently running task is different from the highest priority task
bool should_update_priorities() {
  TMB_t       *task_highest_priority = produce_highest_priority_task();
  TaskHandle_t current_task          = xTaskGetCurrentTaskHandle();
  return task_highest_priority->handle != current_task;
}

void updatePriorities() {
  if (!should_update_priorities()) {
    return;
  }
  deprioritizeAllTasks();
  resumeAllTasks();
  setHighestPriority();
}

void vApplicationTickHook(void) {
  setSchedulable();
  updatePriorities();
}

// TODO: This function should be able to accept a pointer to a task's TMB, which would reduce the number of parameters
// needed.
// clang-format off
void record_trace_event(
  TraceEventType_t event,
  TraceTaskType_t task_type,
  uint8_t task_id,
  uint8_t resource_id,
  unsigned int preemption_level,
  TickType_t deadline
)
// clang-format on
{
  if (trace_count < MAX_TRACE_RECORDS) {
    trace_buffer[trace_count].timestamp   = xTaskGetTickCount();
    trace_buffer[trace_count].event_type  = event;
    trace_buffer[trace_count].task_type   = task_type;
    trace_buffer[trace_count].task_id     = task_id;
    trace_buffer[trace_count].resource_id = resource_id;

    trace_buffer[trace_count].system_ceiling =
      get_srp_system_ceiling(); // Grab current ceiling dynamically
    trace_buffer[trace_count].preempt_level = preemption_level;
    trace_buffer[trace_count].deadline      = deadline;

    trace_count++;
  }
}

void task_switched_out(void) {
#if TRACE_WITH_LOGIC_ANALYZER
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  TaskHandle_t idle_task    = xTaskGetIdleTaskHandle();

  if (current_task == NULL)
    return;

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
    record_trace_event(TRACE_EVENT_SWITCH_IN, TRACE_TASK_IDLE, 0, 0, 0, 0);
    return;
  }

  TMB_t *current_task_tmb = get_task_by_handle(current_task);
  if (current_task_tmb == NULL) {
    // This should never happen, but just in case
    record_trace_event(TRACE_EVENT_SWITCH_IN, TRACE_TASK_SYSTEM, 0, 0, 0, 0);
    return;
  }

  if (current_task_tmb->type == TASK_PERIODIC) {
    record_trace_event(
      TRACE_EVENT_SWITCH_IN, TRACE_TASK_PERIODIC, current_task_tmb->id + 1, 0, current_task_tmb->preemption_level,
      current_task_tmb->absolute_deadline
    );
    return;
  } else if (current_task_tmb->type == TASK_APERIODIC) {
    record_trace_event(
      TRACE_EVENT_SWITCH_IN, TRACE_TASK_APERIODIC, current_task_tmb->id + 1, 0, current_task_tmb->preemption_level,
      current_task_tmb->absolute_deadline
    );
    return;
  }
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
    TickType_t currentTick = xTaskGetTickCount();
    if (currentTick != previousTick) {
      previousTick = currentTick;
      xTimeSlicesExecutedThusFar++;
    }
    if (xTimeSlicesExecutedThusFar == xCompletionTime) {
      xTimeSlicesExecutedThusFar = 0;
      taskPeriodicDone(xTaskGetCurrentTaskHandle());
      vTaskSuspend(NULL);
    }
    // vTaskDelay(pdMS_TO_TICKS(200));
  }
}

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
