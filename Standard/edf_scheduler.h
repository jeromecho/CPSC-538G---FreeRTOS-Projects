#ifndef EDF_SCHEDULER_H
#define EDF_SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"

#define PRIORITY_NOT_DONE_RUNNING     (tskIDLE_PRIORITY + 2)
#define PRIORITY_NOT_DONE_NOT_RUNNING (tskIDLE_PRIORITY + 1)
#define PRIORITY_IDLE                 (tskIDLE_PRIORITY)

typedef struct TMB_t {
  TickType_t   absolute_deadline;
  TickType_t   completion_time;
  TaskHandle_t handle;
} TMB_t;

typedef struct TMB_Periodic_t {
  TMB_t      tmb;
  TickType_t period;
  TickType_t next_period;
  TickType_t relative_deadline;
  TickType_t release_time;
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
  TaskFunction_t pxTaskCode, const char *const pcName, const configSTACK_DEPTH_TYPE uxStackDepth,
  void *const pvParameters, TickType_t xPeriod, TickType_t xDeadlineRelative,
  TaskHandle_t *const pxCreatedTask
);

BaseType_t xTaskCreateAperiodic(
  TaskFunction_t pxTaskCode, const char *const pcName, const configSTACK_DEPTH_TYPE uxStackDepth,
  void *const pvParameters, TaskHandle_t *const pxCreatedTask
);

#define MAXIMUM_PERIODIC_TASKS  100
#define MAXIMUM_APERIODIC_TASKS 100

extern TMB_Periodic_t periodic_tasks[MAXIMUM_PERIODIC_TASKS];
extern size_t         periodic_task_count;

extern TMB_Aperiodic_t aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
extern size_t          aperiodic_task_count;

#endif // EDF_SCHEDULER_H
