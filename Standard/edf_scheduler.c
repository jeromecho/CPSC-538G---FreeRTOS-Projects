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

TMB_Periodic_t periodic_tasks[MAXIMUM_PERIODIC_TASKS];
size_t         periodic_task_count = 0;

TMB_Aperiodic_t aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
size_t          aperiodic_task_count = 0;

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
    TMB_Periodic_t *task = &periodic_tasks[i];
    if (task->tmb.is_done) {
      TickType_t current_tick = xTaskGetTickCount();
      // TODO: This should be "if the last *period* has passed", not just the last deadline.
      // If the last deadline has passed, set a new deadline
      if (current_tick >= task->next_period) {
        task->tmb.absolute_deadline = task->next_period + task->relative_deadline;
        task->next_period           = task->next_period + task->period;
        task->tmb.is_done           = false;
        record_trace_event(
          TRACE_EVENT_RELEASE, TRACE_TASK_PERIODIC, i + 1, 0, 0, task->tmb.absolute_deadline
        );
        // vTaskResume(task->tmb.handle); // Shouldn't matter wether the task is already running
      }
    }
  }
}

/// @brief Return task handle of highest priority task in TMB arrays. Return NULL if none
TaskHandle_t produce_highest_priority_task() {
  // Iterate through all periodic tasks and find the one with the nearest deadline
  TMB_Periodic_t *candidate_periodic_task = NULL;
  for (size_t i = 0; i < periodic_task_count; ++i) {
    if (!periodic_tasks[i].tmb.is_done) {
      if (candidate_periodic_task == NULL || periodic_tasks[i].tmb.absolute_deadline <
                                               candidate_periodic_task->tmb.absolute_deadline) {
        candidate_periodic_task = &periodic_tasks[i];
      }
    }
  }

  // Iterate through all aperiodic tasks and find the one with the nearest deadline
  TMB_Aperiodic_t *candidate_aperiodic_task = NULL;
  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    if (!aperiodic_tasks[i].tmb.is_done) {
      if (candidate_aperiodic_task == NULL || aperiodic_tasks[i].tmb.absolute_deadline <
                                                candidate_aperiodic_task->tmb.absolute_deadline) {
        candidate_aperiodic_task = &aperiodic_tasks[i];
      }
    }
  }

  // // Early return if there are no tasks available
  if (candidate_periodic_task == NULL && candidate_aperiodic_task == NULL) {
    return NULL;
  }

  TickType_t periodic_deadline  = (candidate_periodic_task != NULL)
                                    ? candidate_periodic_task->tmb.absolute_deadline
                                    : (TickType_t)(UINT32_MAX);
  TickType_t aperiodic_deadline = (candidate_aperiodic_task != NULL)
                                    ? candidate_aperiodic_task->tmb.absolute_deadline
                                    : (TickType_t)(UINT32_MAX);

  TaskHandle_t candidate_task             = NULL;
  unsigned int candidate_preemption_level = 0;
  if (periodic_deadline < aperiodic_deadline) {
    candidate_task             = candidate_periodic_task->tmb.handle;
    candidate_preemption_level = candidate_periodic_task->tmb.preemption_level;
  } else {
    candidate_task             = candidate_aperiodic_task->tmb.handle;
    candidate_preemption_level = candidate_aperiodic_task->tmb.preemption_level;
  }

// --- SRP PREEMPTION CHECK ---
#if USE_SRP
  configASSERT(srp_is_initialized());
  TaskHandle_t          currently_running_task = xTaskGetCurrentTaskHandle();
  volatile unsigned int current_system_ceiling = get_srp_system_ceiling();
  if (candidate_preemption_level <= current_system_ceiling) {
    return currently_running_task;
  }
#endif

  return candidate_task;
}

/// @brief Iterates through tasks and set the highest priority to the task with the nearest deadline
void setHighestPriority() {
  TaskHandle_t earliest_task = produce_highest_priority_task();

  if (earliest_task != NULL) {
    // Set the priority of the task with the nearest deadline to the highest priority
    vTaskPrioritySet(earliest_task, PRIORITY_NOT_DONE_RUNNING);
  }
}

void deprioritizeAllTasks() {
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];
    if (!task->tmb.is_done) {
      vTaskPrioritySet(task->tmb.handle, PRIORITY_NOT_DONE_NOT_RUNNING);
    }
  }

  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    TMB_Aperiodic_t *task = &aperiodic_tasks[i];
    if (!task->tmb.is_done) {
      vTaskPrioritySet(task->tmb.handle, PRIORITY_NOT_DONE_NOT_RUNNING);
    }
  }
}

void resumeAllTasks() {
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];

    if (!task->tmb.is_done && task->tmb.release_time <= xTaskGetTickCount()) {
      vTaskResume(task->tmb.handle);
    }
  }

  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    TMB_Aperiodic_t *task = &aperiodic_tasks[i];
    if (!task->tmb.is_done && task->tmb.release_time <= xTaskGetTickCount()) {
      vTaskResume(task->tmb.handle);
    }
  }
}

void taskPeriodicDone(TaskHandle_t task_handle) {
  taskENTER_CRITICAL();

  // Mark the task as done
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];
    if (task->tmb.handle == task_handle) {
      task->tmb.is_done = true;
      if (xTaskGetTickCount() > task->tmb.absolute_deadline) {
        printf("Resetting system...\n");
        // Allow the UART to flush the message before resetting
        busy_wait_us_32(5000);
        // reboot - wait for reboot in low power mode
        watchdog_reboot(0, 0, 0);
        while (1) {
          __wfi();
        };
      }
      // gpio_put(mainGPIO_LED_TASK_1, 1);
      break;
    }
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
    TMB_Periodic_t *new_task      = &periodic_tasks[periodic_task_count++];
    new_task->tmb.handle          = task_handle;
    new_task->period              = xPeriod;
    new_task->relative_deadline   = xDeadlineRelative;
    new_task->tmb.is_done         = false;
    new_task->tmb.completion_time = (TickType_t)pvParameters;

    if (isSchedulerStarted) {
      new_task->tmb.release_time      = xTaskGetTickCount();
      new_task->next_period           = xTaskGetTickCount() + xPeriod;
      new_task->tmb.absolute_deadline = xTaskGetTickCount() + new_task->relative_deadline;
    } else {
      TickType_t release_time         = calculate_release_time_for_dropped_task(new_task->period);
      new_task->tmb.release_time      = release_time;
      new_task->next_period           = release_time + xPeriod;
      new_task->tmb.absolute_deadline = release_time + new_task->relative_deadline;
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
// BaseType_t xTaskCreateAperiodic(
//   TaskFunction_t pxTaskCode, const char *const pcName, const configSTACK_DEPTH_TYPE uxStackDepth,
//   void *const pvParameters, TaskHandle_t *const pxCreatedTask
// ) {
//   if (aperiodic_task_count >= MAXIMUM_APERIODIC_TASKS) {
//     return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
//   }

//   BaseType_t result = xTaskCreate(
//     pxTaskCode, pcName, uxStackDepth, pvParameters, PRIORITY_NOT_DONE_NOT_RUNNING, pxCreatedTask
//   );

//   if (result == pdPASS) {
//     TMB_Aperiodic_t *new_task     = &aperiodic_tasks[aperiodic_task_count++];
//     new_task->tmb.handle          = *pxCreatedTask;
//     new_task->tmb.completion_time = (TickType_t)pvParameters;
//   }

//   return result;
// }
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
  BaseType_t   result             = xTaskCreate(
    pxTaskCode, pcName, uxStackDepth, pvParameters, PRIORITY_NOT_DONE_NOT_RUNNING, &task_handle
  );

  if (result == pdPASS) {
    TMB_Aperiodic_t *new_task     = &aperiodic_tasks[aperiodic_task_count++];
    new_task->tmb.handle          = task_handle;
    new_task->tmb.completion_time = (TickType_t)pvParameters;
    new_task->tmb.is_done         = false;

    // 1. Initialize EDF scheduling fields
    new_task->tmb.release_time      = xReleaseTime;
    new_task->tmb.absolute_deadline = xReleaseTime + xDeadlineRelative;

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
  TaskHandle_t task_highest_priority = produce_highest_priority_task();
  TaskHandle_t task_current          = xTaskGetCurrentTaskHandle();
  return task_highest_priority != task_current;
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

  // Check periodic tasks
  for (size_t i = 0; i < periodic_task_count; ++i) {
    if (current_task == periodic_tasks[i].tmb.handle) {
      record_trace_event(
        TRACE_EVENT_SWITCH_IN, TRACE_TASK_PERIODIC, i + 1, 0,
        periodic_tasks[i].tmb.preemption_level, periodic_tasks[i].tmb.absolute_deadline
      );
      return;
    }
  }

  // Check aperiodic tasks
  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    if (current_task == aperiodic_tasks[i].tmb.handle) {
      record_trace_event(
        TRACE_EVENT_SWITCH_IN, TRACE_TASK_APERIODIC, i + 1, 0,
        aperiodic_tasks[i].tmb.preemption_level, aperiodic_tasks[i].tmb.absolute_deadline
      );
      return;
    }
  }

  // Catch-all for monitor/system tasks
  record_trace_event(TRACE_EVENT_SWITCH_IN, TRACE_TASK_SYSTEM, 0, 0, 0, 0);
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
