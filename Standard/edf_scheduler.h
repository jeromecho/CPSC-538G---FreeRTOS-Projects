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

// TODO: Move trace functionality into its own file

#define MAX_TRACE_RECORDS         300
#define TRACE_WITH_LOGIC_ANALYZER false

typedef enum {
  TRACE_EVENT_SWITCH_IN = 0,
  TRACE_EVENT_RELEASE, // When a task wakes up / is ready
  TRACE_EVENT_SEMAPHORE_TAKE,
  TRACE_EVENT_SEMAPHORE_GIVE,
  TRACE_EVENT_SRP_BLOCK, // When a task is ready but denied CPU due to ceiling
  TRACE_EVENT_DEADLINE_MISS,
  TRACE_EVENT_SWITCH_OUT,
  TRACE_EVENT_UPDATING_PRIORITIES,
  TRACE_EVENT_DEPRIORITIZED,
  TRACE_EVENT_PRIORITY_SET,
  TRACE_EVENT_DONE, // When a task finishes execution
  TRACE_EVENT_RESCHEDULED,
} TraceEventType_t;

typedef enum {
  TRACE_TASK_IDLE = 0,
  TRACE_TASK_PERIODIC,
  TRACE_TASK_APERIODIC,
  TRACE_TASK_SYSTEM, // For any other task that doesn't fit the above categories
  TRACE_TASK_EITHER, // The trace function will automatically determine if this is a periodic or aperiodic task
} TraceTaskType_t;

typedef struct {
  TickType_t       FreeRTOS_tick;
  TraceEventType_t event_type;
  absolute_time_t  time; // Absolute time for better resolution in visualization

  TraceTaskType_t task_type;
  uint8_t         task_id;  // e.g., 0, 1, 2 for the specific task array index
  UBaseType_t     priority; // Captured priority at the time of the event
  eTaskState      task_state;


  // --- Contextual Data ---
  uint8_t      resource_id;    // Which semaphore was taken/given (if applicable)
  unsigned int system_ceiling; // The SRP ceiling AT THE TIME of this event
  unsigned int preempt_level;  // Preemption level of the acting task
  TickType_t   deadline;       // Absolute deadline of the acting task (for EDF checks)
} TraceRecord_t;

extern TraceRecord_t trace_buffer[MAX_TRACE_RECORDS];
extern size_t        trace_count;

void record_trace_event( //
  const TraceEventType_t event,
  TraceTaskType_t        task_type,
  const TMB_t *const     task,
  const uint8_t          resource_id
);

void print_trace_buffer();

#endif // EDF_SCHEDULER_H
