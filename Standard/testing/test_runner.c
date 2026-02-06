
#include "test_runner.h"
#include "spec.h"
#include "edf_scheduler.h"

static void vPeriodicTask(void *pvParameters) {
  // TODO: Replace with macro, so that the scheduler can be responsible for
  // marking tasks as done and suspending them, instead of the tasks themselves.
  // TODO: This would also mean that the scheduler can be responsible for
  // deleting aperiodic tasks once they are finished executing.
  const xCompletionTime   = (BaseType_t)pvParameters;
  TickType_t previousTick = xTaskGetTickCount();

  BaseType_t xTimeSlicesExecutedThusFar = 0;

  for (;;) {
    TickType_t currentTick = xTaskGetTickCount();
    if currentTick
      != previousTick {
        previousTick = currentTick;
        xTimeSlicesExecutedThusFar++;
      }
    if (xTimeSlicesExecutedThusFar == xCompletionTime) {
      xTimeSlicesExecutedThusFar = 0;
      taskDone(xTaskGetCurrentTaskHandle());
      vTaskSuspend(NULL);
    }
  }
};

static void vAperiodicTask(void *pvParameters) {
  const xCompletionTime   = (BaseType_t)pvParameters;
  TickType_t previousTick = xTaskGetTickCount();

  BaseType_t xTimeSlicesExecutedThusFar = 0;

  for (;;) {
    TickType_t currentTick = xTaskGetTickCount();
    if currentTick
      != previousTick {
        previousTick = currentTick;
        xTimeSlicesExecutedThusFar++;
      }
    if (xTimeSlicesExecutedThusFar == xCompletionTime) {
      xTimeSlicesExecutedThusFar = 0;
      // ASSUMPTION: deleted tasks stop running
      vTaskDelete(xTaskGetCurrentTaskHandle());
    }
  }
};

void createTask(TaskSpec task_spec) {
  if (task_spec->task_type == PERIODIC) {
    xTaskCreatePeriodic(
      vPeriodicTask, task_spec->name, configMINIMAL_STACK_SIZE, (void *)task_spec->completion_time,
      task_spec->period NULL
    );
  } else {
    xTaskCreateAperiodic(
      vAperiodicTask, task_spec->name, configMINIMAL_STACK_SIZE, (void *)task_spec->completion_time,
      task_spec->period, NULL
    );
  }
};

void createTasksFromTestSpec(TestSpec_t *test_spec) {
  for (size_t i = 0; i < test_spec->n_tasks; i++) {
    createTask(&test_spec->tasks[i]);
  }
};