#include "edf_scheduler.h"

void setSchedulable() {
  // Iterate through all periodic tasks and set their deadlines
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];
    if (task->is_done) {
      TickType_t current_tick = xTaskGetTickCount();
      // If the last deadline has passed, set a new deadline
      if (current_tick >= task->last_deadline) {
        task->tmb.absolute_deadline = task->last_deadline + task->period;
        task->last_deadline         = task->tmb.absolute_deadline;
        task->is_done               = false;
      }
    }
  }
}

/// @brief Iterates through tasks and set the highest priority to the task with the nearest deadline
void setHighestPriority() {
  // Iterate through all periodic tasks and find the one with the nearest deadline
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

  // Iterate through all aperiodic tasks and find the one with the nearest deadline
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
  if (earliest_deadline_periodic_task == NULL || earliest_deadline_aperiodic_task == NULL) {
    if (earliest_deadline_periodic_task != NULL) {
      earliest_task = earliest_deadline_periodic_task;
    } else if (earliest_deadline_aperiodic_task != NULL) {
      earliest_task = earliest_deadline_aperiodic_task;
    } else {
      // No tasks available
      earliest_task = NULL;
    }
  } else {
    TickType_t periodic_deadline  = earliest_deadline_periodic_task->tmb.absolute_deadline;
    TickType_t aperiodic_deadline = earliest_deadline_aperiodic_task->tmb.absolute_deadline;
    earliest_task                 = (periodic_deadline < aperiodic_deadline)
                                      ? earliest_deadline_periodic_task
                                      : earliest_deadline_aperiodic_task;
  }

  if (earliest_task != NULL) {
    // Set the highest priority to the task with the nearest deadline
    vTaskPrioritySet(earliest_task, configMAX_PRIORITIES - 1);
  }
}

void deprioritizeAllTasks() {
  // Tasks which are not done get a priority of 2
  // Idle task gets priority of 1
  // All other tasks get priority of 0
  for (size_t i = 0; i < periodic_task_count; ++i) {
    TMB_Periodic_t *task = &periodic_tasks[i];
    if (!task->is_done) {
      vTaskPrioritySet(task->tmb.handle, 2);
    } else {
      vTaskPrioritySet(task->tmb.handle, 0);
    }
  }

  for (size_t i = 0; i < aperiodic_task_count; ++i) {
    TMB_Aperiodic_t *task = &aperiodic_tasks[i];
    if (!task->is_done) {
      vTaskPrioritySet(task->tmb.handle, 2);
    } else {
      vTaskPrioritySet(task->tmb.handle, 0);
    }
  }
}

void updatePriorities() {
  deprioritizeAllTasks();
  setHighestPriority();
}
