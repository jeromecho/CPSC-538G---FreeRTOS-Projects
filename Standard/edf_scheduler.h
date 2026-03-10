#ifndef EDF_SCHEDULER_H
#define EDF_SCHEDULER_H

#include "FreeRTOS.h" // IWYU pragma: keep
#include "ProjectConfig.h"
#include "task.h"

#define PRIORITY_RUNNING     (tskIDLE_PRIORITY + 2)
#define PRIORITY_NOT_RUNNING (tskIDLE_PRIORITY + 1)
#define PRIORITY_IDLE        (tskIDLE_PRIORITY)

#define errADMISSION_FAILED (-6)

typedef enum { TASK_PERIODIC, TASK_APERIODIC } TaskType_t;

typedef struct TMB_t {
  // --- Common Metadata ---
  TaskType_t   type;
  size_t       id; // Index in the corresponding TMB array, starting from 0
  TaskHandle_t handle;
  bool         is_done;

  // --- Common Scheduling Data ---
  TickType_t release_time;
  TickType_t absolute_deadline;
  TickType_t completion_time;

  // --- SRP-specific Data ---
#if USE_SRP
  unsigned int preemption_level; // Only used for SRP, but it is more convenient to just store it in
                                 // the TMB than to have a separate data structure for SRP tasks
#endif

  // --- Type-Specific Data ---
  union {
    struct {
      TickType_t period;
      TickType_t relative_deadline;
      TickType_t next_period;
    } periodic;

    struct {
    } aperiodic;
  };
} TMB_t;

BaseType_t EDF_create_periodic_task(
  TaskFunction_t               pxTaskCode,
  const char *const            pcName,
  const configSTACK_DEPTH_TYPE uxStackDepth,
  const TickType_t             completionTime,
  const TickType_t             xPeriod,
  const TickType_t             xDeadlineRelative,
  TaskHandle_t *const          pxCreatedTask
);

BaseType_t EDF_create_aperiodic_task(
  TaskFunction_t               pxTaskCode,
  const char *const            pcName,
  const configSTACK_DEPTH_TYPE uxStackDepth,
  const TickType_t             completionTime,
  const TickType_t             xReleaseTime,
  const TickType_t             xDeadlineRelative,
  TaskHandle_t *const          pxCreatedTask
);

void EDF_periodic_task(void *pvParameters);

TMB_t *EDF_produce_highest_priority_task();
TMB_t *EDF_get_task_by_handle(TaskHandle_t handle);
void   EDF_mark_task_done(TaskHandle_t task_handle);
void   EDF_scheduler_start();

#define MAXIMUM_PERIODIC_TASKS  100
#define MAXIMUM_APERIODIC_TASKS 100

extern TMB_t  periodic_tasks[MAXIMUM_PERIODIC_TASKS];
extern size_t periodic_task_count;

extern TMB_t  aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
extern size_t aperiodic_task_count;

#endif // EDF_SCHEDULER_H
