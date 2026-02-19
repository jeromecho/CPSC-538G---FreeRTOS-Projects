#include "edf_scheduler.h"
#include "hardware/gpio.h"
#include "main_blinky.h"
#include "admission_control.h"
#include <stdio.h>

TMB_Periodic_t periodic_tasks[MAXIMUM_PERIODIC_TASKS];
size_t         periodic_task_count = 0;

TMB_Aperiodic_t aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
size_t          aperiodic_task_count = 0;

void setSchedulable() {
  // Iterate through all periodic tasks and set their deadlines
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];
    if (task->is_done) {
      TickType_t current_tick = xTaskGetTickCount();
      // TODO: This should be "if the last *period* has passed", not just the last deadline.
      // If the last deadline has passed, set a new deadline
      if (current_tick >= task->next_period) {
        task->tmb.absolute_deadline = task->next_period + task->relative_deadline;
        task->next_period           = task->next_period + task->period;
        task->is_done               = false;
        // vTaskResume(task->tmb.handle); // Shouldn't matter wether the task is already running
      }
    }
  }
}

/// @brief Return task handle of highest priority task in TMB arrays. Return NULL if none
TaskHandle_t produce_highest_priority_task() {
  // Iterate through all periodic tasks and find the one with the nearest deadline
  TMB_Periodic_t *edf_periodic_task = NULL;
  for (size_t i = 0; i < periodic_task_count; ++i) {
    if (!periodic_tasks[i].is_done) {
      if (edf_periodic_task == NULL ||
          periodic_tasks[i].tmb.absolute_deadline < edf_periodic_task->tmb.absolute_deadline) {
        edf_periodic_task = &periodic_tasks[i];
      }
    }
  }

  // Iterate through all aperiodic tasks and find the one with the nearest
  // deadline
  TMB_Aperiodic_t *edf_aperiodic_task = NULL;
  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    if (edf_aperiodic_task == NULL ||
        aperiodic_tasks[i].tmb.absolute_deadline < edf_aperiodic_task->tmb.absolute_deadline) {
      edf_aperiodic_task = &aperiodic_tasks[i];
    }
  }

  // Determine which of the two tasks has the earliest deadline
  TaskHandle_t earliest_task = NULL;
  if (edf_periodic_task == NULL || edf_aperiodic_task == NULL) {
    if (edf_periodic_task != NULL) {
      earliest_task = edf_periodic_task->tmb.handle;
    } else if (edf_aperiodic_task != NULL) {
      earliest_task = edf_aperiodic_task->tmb.handle;
    } else {
      // No tasks available
      earliest_task = NULL;
    }
  } else {
    TickType_t periodic_deadline  = edf_periodic_task->tmb.absolute_deadline;
    TickType_t aperiodic_deadline = edf_aperiodic_task->tmb.absolute_deadline;
    earliest_task = (periodic_deadline < aperiodic_deadline) ? edf_periodic_task->tmb.handle
                                                             : edf_aperiodic_task->tmb.handle;
  }
  return earliest_task;
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
    if (!task->is_done) {
      vTaskPrioritySet(task->tmb.handle, PRIORITY_NOT_DONE_NOT_RUNNING);
    } else {
      // vTaskSuspend(task->tmb.handle);
    }
  }

  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    TMB_Aperiodic_t *task = &aperiodic_tasks[i];
    vTaskPrioritySet(task->tmb.handle, PRIORITY_NOT_DONE_NOT_RUNNING);
  }
}

void resumeAllTasks() {
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];
    if (!task->is_done) {
      vTaskResume(task->tmb.handle);
    }
  }

  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    TMB_Aperiodic_t *task = &aperiodic_tasks[i];
    vTaskResume(task->tmb.handle);
  }
}

void taskDone(TaskHandle_t task_handle) {
  taskENTER_CRITICAL();

  // Mark the task as done
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];
    if (task->tmb.handle == task_handle) {
      task->is_done = true;
      // gpio_put(mainGPIO_LED_TASK_1, 1);
      break;
    }
  }

  taskEXIT_CRITICAL();

  // for (size_t i = 0; i < aperiodic_task_count; ++i) {
  //   TMB_Aperiodic_t *task = &aperiodic_tasks[i];
  //   if (task->tmb.handle == task_handle) {
  //     // Aperiodic tasks are considered done immediately
  //     return;
  //   }
  // }
}

// REQUIRES: xDeadlinePeriodic <= xPeriod must hold
BaseType_t xTaskCreatePeriodic(
  TaskFunction_t pxTaskCode, const char *const pcName, const configSTACK_DEPTH_TYPE uxStackDepth,
  void *const pvParameters, TickType_t xPeriod, TickType_t xDeadlineRelative,
  TaskHandle_t *const pxCreatedTask
) {
  if (periodic_task_count >= MAXIMUM_PERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }

  if (!can_admit_periodic_task((TickType_t)pvParameters, xPeriod, xDeadlineRelative)) {
    // TODO: comment out the bottom as necessary
    printf("xTaskCreatePeriodic - admission failed\n");
    return errADMISSION_FAILED;
  }
  // TODO: comment out the bottom as necessary
  printf("xTaskCreatePeriodic - admission: %s successed\n", pcName);

  configASSERT(xDeadlineRelative <= xPeriod);

  TaskHandle_t task_handle;
  BaseType_t result = xTaskCreate(pxTaskCode, pcName, uxStackDepth, pvParameters, 2, &task_handle);

  // TODO: it might be a bit redundant to pass completion time as a reference and also
  // pass it as a parameter to created task (a single handle to TMB might be sufficient)
  if (result == pdPASS) {
    TMB_Periodic_t *new_task        = &periodic_tasks[periodic_task_count++];
    new_task->tmb.handle            = task_handle;
    new_task->period                = xPeriod;
    new_task->next_period           = xTaskGetTickCount() + xPeriod;
    new_task->relative_deadline     = xDeadlineRelative;
    new_task->tmb.absolute_deadline = xTaskGetTickCount() + new_task->relative_deadline;
    new_task->tmb.completion_time   = (TickType_t)pvParameters;
    new_task->is_done               = false;

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
  TaskFunction_t pxTaskCode, const char *const pcName, const configSTACK_DEPTH_TYPE uxStackDepth,
  void *const pvParameters, TaskHandle_t *const pxCreatedTask
) {
  if (aperiodic_task_count >= MAXIMUM_APERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }

  BaseType_t result = xTaskCreate(
    pxTaskCode, pcName, uxStackDepth, pvParameters, PRIORITY_NOT_DONE_NOT_RUNNING, pxCreatedTask
  );

  if (result == pdPASS) {
    TMB_Aperiodic_t *new_task     = &aperiodic_tasks[aperiodic_task_count++];
    new_task->tmb.handle          = *pxCreatedTask;
    new_task->tmb.completion_time = (TickType_t)pvParameters;
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
