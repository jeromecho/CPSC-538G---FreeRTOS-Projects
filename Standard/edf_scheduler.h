#ifndef EDF_SCHEDULER_H
#define EDF_SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"

#define PRIORITY_NOT_DONE_RUNNING     (tskIDLE_PRIORITY + 2)
#define PRIORITY_NOT_DONE_NOT_RUNNING (tskIDLE_PRIORITY + 1)
#define PRIORITY_IDLE                 (tskIDLE_PRIORITY)

#define errADMISSION_FAILED (-6)

typedef struct TMB_t {
  TickType_t   absolute_deadline;
  TickType_t   completion_time;
  unsigned int preemption_level; // Only used for SRP, but it is more convenient to just store it in
                                 // the TMB than to have a separate data structure for SRP tasks
  TickType_t   release_time;
  bool         is_done;
  TaskHandle_t handle;
} TMB_t;

typedef struct TMB_Periodic_t {
  TMB_t      tmb;
  TickType_t period;
  TickType_t next_period;
  TickType_t relative_deadline;
} TMB_Periodic_t;

typedef struct TMB_Aperiodic_t {
  TMB_t tmb;
} TMB_Aperiodic_t;

void taskPeriodicDone(TaskHandle_t task_handle);

BaseType_t xTaskCreatePeriodic(
  TaskFunction_t pxTaskCode, const char *const pcName, const configSTACK_DEPTH_TYPE uxStackDepth,
  void *const pvParameters, TickType_t xPeriod, TickType_t xDeadlineRelative,
  TaskHandle_t *const pxCreatedTask
);

BaseType_t xTaskCreateAperiodic(
  TaskFunction_t pxTaskCode, const char *const pcName, const configSTACK_DEPTH_TYPE uxStackDepth,
  void *const pvParameters, TickType_t xReleaseTime, TickType_t xDeadlineRelative,
  TaskHandle_t *const pxCreatedTask
);

void vPeriodicTask(void *pvParameters);

TaskHandle_t produce_highest_priority_task();

#define MAXIMUM_PERIODIC_TASKS  100
#define MAXIMUM_APERIODIC_TASKS 100

extern TMB_Periodic_t periodic_tasks[MAXIMUM_PERIODIC_TASKS];
extern size_t         periodic_task_count;

extern TMB_Aperiodic_t aperiodic_tasks[MAXIMUM_APERIODIC_TASKS];
extern size_t          aperiodic_task_count;

#define MAX_TRACE_RECORDS         250
#define TRACE_WITH_LOGIC_ANALYZER false

// typedef struct {
//   TickType_t timestamp;
//   uint8_t    task_id; // 0 = Idle, 1 = Task 1, etc.
// } TraceRecord_t;

typedef enum {
  TRACE_EVENT_SWITCH_IN = 0,
  TRACE_EVENT_RELEASE, // When a task wakes up / is ready
  TRACE_EVENT_SEMAPHORE_TAKE,
  TRACE_EVENT_SEMAPHORE_GIVE,
  TRACE_EVENT_SRP_BLOCK, // When a task is ready but denied CPU due to ceiling
  TRACE_EVENT_DEADLINE_MISS
} TraceEventType_t;

typedef enum {
  TRACE_TASK_IDLE = 0,
  TRACE_TASK_PERIODIC,
  TRACE_TASK_APERIODIC,
  TRACE_TASK_SYSTEM // For any other task that doesn't fit the above categories
} TraceTaskType_t;

// The expanded record struct
typedef struct {
  TickType_t       timestamp;
  TraceEventType_t event_type;

  TraceTaskType_t task_type;
  uint8_t         task_id; // e.g., 0, 1, 2 for the specific task array index

  // --- Contextual Data ---
  uint8_t      resource_id;    // Which semaphore was taken/given (if applicable)
  unsigned int system_ceiling; // The SRP ceiling AT THE TIME of this event
  unsigned int preempt_level;  // Preemption level of the acting task
  TickType_t   deadline;       // Absolute deadline of the acting task (for EDF checks)
} TraceRecord_t;

void record_trace_event(
  TraceEventType_t event, TraceTaskType_t task_type, uint8_t task_id, uint8_t resource_id,
  unsigned int preemption_level, TickType_t deadline
);

extern TraceRecord_t trace_buffer[MAX_TRACE_RECORDS];
extern size_t        trace_count;

#endif // EDF_SCHEDULER_H
