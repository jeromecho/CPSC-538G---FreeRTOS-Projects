#ifndef EDF_SCHEDULER_H
#define EDF_SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"

#define PRIORITY_NOT_DONE_RUNNING     3
#define PRIORITY_NOT_DONE_NOT_RUNNING 2
#define PRIORITY_IDLE                 1
#define PRIORITY_DONE                 0

typedef struct TMB_t {
  TickType_t   absolute_deadline;
  TaskHandle_t handle;
} TMB_t;

typedef struct TMB_Periodic_t {
  TMB_t      tmb;
  TickType_t period;
  TickType_t last_deadline;
  bool       is_done;
} TMB_Periodic_t;

typedef struct TMB_Aperiodic_t {
  TMB_t tmb;
} TMB_Aperiodic_t;

void setSchedulable();
void updatePriorities();
void deprioritizeAllTasks();

void taskDone(TaskHandle_t task_handle);

BaseType_t xTaskCreatePeriodic(
  TaskFunction_t               pxTaskCode,
  const char *const            pcName,
  const configSTACK_DEPTH_TYPE uxStackDepth,
  void *const                  pvParameters,
  TickType_t                   xPeriod,
  TaskHandle_t *const          pxCreatedTask
);

BaseType_t xTaskCreateAperiodic(
  TaskFunction_t               pxTaskCode,
  const char *const            pcName,
  const configSTACK_DEPTH_TYPE uxStackDepth,
  void *const                  pvParameters,
  TaskHandle_t *const          pxCreatedTask
);

const size_t MAXIMUM_PERIODIC_TASKS  = 5;
const size_t MAXIMUM_APERIODIC_TASKS = 5;

static TMB_Periodic_t periodic_tasks[MAXIMUM_PERIODIC_TASKS];
static size_t         periodic_task_count = 0;

static TMB_Aperiodic_t aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
static size_t          aperiodic_task_count = 0;

#endif // EDF_SCHEDULER_H
