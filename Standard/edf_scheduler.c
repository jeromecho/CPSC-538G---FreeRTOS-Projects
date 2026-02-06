#include "edf_scheduler.h"

void setSchedulable() {
  // Iterate through all periodic tasks and set their deadlines
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];
    if (task->is_done) {
      TickType_t current_tick = xTaskGetTickCount();
      // TODO: This should be "if the last *period* has passed", not just the
      // last deadline. If the last deadline has passed, set a new deadline
      if (current_tick >= task->last_deadline) {
        task->tmb.absolute_deadline = task->last_deadline + task->period;
        task->last_deadline = task->tmb.absolute_deadline;
        task->is_done = false;
        vTaskResume(
            task->tmb
                .handle); // Shouldn't matter wether the task is already running
      }
    }
  }
}

/**
 @brief Iterates through tasks and set the highest priority to the task with
 the nearest deadline
 */
void setHighestPriority() {
  // Iterate through all periodic tasks and find the one with the nearest
  // deadline
  TaskHandle_t earliest_deadline_periodic_task = NULL;
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *periodic_task = &periodic_tasks[i];
    if (!periodic_task->is_done) {
      if (earliest_deadline_periodic_task == NULL ||
          periodic_task->tmb.absolute_deadline <
              periodic_tasks[i].tmb.absolute_deadline) {
        earliest_deadline_periodic_task = periodic_task->tmb.handle;
      }
    }
  }

  // Iterate through all aperiodic tasks and find the one with the nearest
  // deadline
  TaskHandle_t earliest_deadline_aperiodic_task = NULL;
  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    TMB_Aperiodic_t *aperiodic_task = &aperiodic_tasks[i];
    if (earliest_deadline_aperiodic_task == NULL ||
        aperiodic_task->tmb.absolute_deadline <
            aperiodic_tasks[i].tmb.absolute_deadline) {
      earliest_deadline_aperiodic_task = aperiodic_task->tmb.handle;
    }
  }

  // Determine which of the two tasks has the earliest deadline
  TaskHandle_t earliest_task = NULL;
  if (earliest_deadline_periodic_task == NULL ||
      earliest_deadline_aperiodic_task == NULL) {
    if (earliest_deadline_periodic_task != NULL) {
      earliest_task = earliest_deadline_periodic_task;
    } else if (earliest_deadline_aperiodic_task != NULL) {
      earliest_task = earliest_deadline_aperiodic_task;
    } else {
      // No tasks available
      earliest_task = NULL;
    }
  } else {
    TickType_t periodic_deadline =
        earliest_deadline_periodic_task->tmb.absolute_deadline;
    TickType_t aperiodic_deadline =
        earliest_deadline_aperiodic_task->tmb.absolute_deadline;
    earliest_task = (periodic_deadline < aperiodic_deadline)
                        ? earliest_deadline_periodic_task
                        : earliest_deadline_aperiodic_task;
  }

  if (earliest_task != NULL) {
    // Set the priority of the task with the nearest deadline to the highest
    // priority
    vTaskPrioritySet(earliest_task, PRIORITY_NOT_DONE_RUNNING);
  }
}

void deprioritizeAllTasks() {
  // Tasks which are not done get a priority of 2
  // Idle task gets priority of 1
  // All other tasks get priority of 0
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];
    if (!task->is_done) {
      vTaskPrioritySet(task->tmb.handle, PRIORITY_NOT_DONE_NOT_RUNNING);
    } else {
      vTaskPrioritySet(task->tmb.handle, PRIORITY_DONE);
    }
  }

  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    TMB_Aperiodic_t *task = &aperiodic_tasks[i];
    if (!task->is_done) {
      vTaskPrioritySet(task->tmb.handle, PRIORITY_NOT_DONE_NOT_RUNNING);
    } else {
      vTaskPrioritySet(task->tmb.handle, PRIORITY_DONE);
    }
  }
}

void resumeAllTasks() {
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];
    vTaskResume(task->tmb.handle);
  }

  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    TMB_Aperiodic_t *task = &aperiodic_tasks[i];
    vTaskResume(task->tmb.handle);
  }
}

void taskDone(TaskHandle_t task_handle) {
  // Mark the task as done
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];
    if (task->tmb.handle == task_handle) {
      task->is_done = true;
      return;
    }
  }

  // for (size_t i = 0; i < aperiodic_task_count; ++i) {
  //   TMB_Aperiodic_t *task = &aperiodic_tasks[i];
  //   if (task->tmb.handle == task_handle) {
  //     // Aperiodic tasks are considered done immediately
  //     return;
  //   }
  // }
}

BaseType_t xTaskCreatePeriodic(TaskFunction_t pxTaskCode,
                               const char *const pcName,
                               const configSTACK_DEPTH_TYPE uxStackDepth,
                               void *const pvParameters, TickType_t xPeriod,
                               TaskHandle_t *const pxCreatedTask) {
  if (periodic_task_count >= MAXIMUM_PERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }

  BaseType_t result = xTaskCreate(pxTaskCode, pcName, uxStackDepth,
                                  pvParameters, 2, pxCreatedTask);

  if (result == pdPASS) {
    TMB_Periodic_t *new_task = &periodic_tasks[periodic_task_count++];
    new_task->tmb.handle = *pxCreatedTask;
    new_task->period = xPeriod;
    new_task->last_deadline = xTaskGetTickCount() + xPeriod;
    new_task->tmb.absolute_deadline = new_task->last_deadline;
    new_task->is_done = false;
  }

  return result;
}

BaseType_t xTaskCreateAperiodic(TaskFunction_t pxTaskCode,
                                const char *const pcName,
                                const configSTACK_DEPTH_TYPE uxStackDepth,
                                void *const pvParameters,
                                TaskHandle_t *const pxCreatedTask) {
  if (aperiodic_task_count >= MAXIMUM_APERIODIC_TASKS) {
    return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
  }

  BaseType_t result =
      xTaskCreate(pxTaskCode, pcName, uxStackDepth, pvParameters,
                  PRIORITY_NOT_DONE_NOT_RUNNING, pxCreatedTask);

  if (result == pdPASS) {
    TMB_Aperiodic_t *new_task = &aperiodic_tasks[aperiodic_task_count++];
    new_task->tmb.handle = *pxCreatedTask;
  }

  return result;
}

void updatePriorities() {
  deprioritizeAllTasks();
  resumeAllTasks();
  setHighestPriority();
}
