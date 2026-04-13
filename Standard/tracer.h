#ifndef TRACER_H
#define TRACER_H

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"
#include "task.h"

typedef enum {
  // Basic events, which are _not_ used together with additional data
  TRACE_RELEASE = 0, // When a task wakes up / is ready
  TRACE_SWITCH_IN,
  TRACE_SWITCH_OUT,
  TRACE_DONE, // When a task finishes execution
  TRACE_RESCHEDULED,
  TRACE_UPDATING_PRIORITIES,
  TRACE_DEPRIORITIZED,
  TRACE_PRIORITY_SET,
  TRACE_DEADLINE_MISS,
  TRACE_SRP_BLOCK, // When a task is ready but denied CPU due to ceiling

  // Non-basic events, which are used when there is additional data provided (like semaphore index)
  TRACE_SEMAPHORE_TAKE,
  TRACE_SEMAPHORE_GIVE,

  // CBS Events
  TRACE_BUDGET_RUN_OUT
} TraceEventType_t;

typedef struct {
  uint32_t index; // The index of the semaphore taken
} EventDataSemaphore_t;
typedef struct {
  TraceEventType_t type;
  union {
    uint8_t semaphore_index;
  } data;
} TraceEvent_t;

#define EVENT_BASIC(event_type)   ((TraceEvent_t){.type = (event_type)})
#define EVENT_SEMAPHORE_TAKE(idx) ((TraceEvent_t){.type = TRACE_SEMAPHORE_TAKE, .data = {.semaphore_index = (idx)}})
#define EVENT_SEMAPHORE_GIVE(idx) ((TraceEvent_t){.type = TRACE_SEMAPHORE_GIVE, .data = {.semaphore_index = (idx)}})

typedef enum {
  TRACE_TASK_IDLE = 0,
  TRACE_TASK_PERIODIC,
  TRACE_TASK_APERIODIC,
  TRACE_TASK_SYSTEM, // For any other task that doesn't fit the above categories
  TRACE_TASK_EITHER, // The trace function will automatically determine if this is a periodic or aperiodic task
} TraceTaskType_t;

typedef struct {
  TickType_t      FreeRTOS_tick;
  TraceEvent_t    event;
  absolute_time_t time; // Absolute time for better resolution in visualization

  TraceTaskType_t task_type;
  uint8_t         task_id;    // e.g., 0, 1, 2 for the specific task array index
  UBaseType_t     priority;   // Captured priority at the time of the event
  eTaskState      task_state; // e.g. ready, suspended.

  // --- Contextual Data ---
  uint8_t      resource_id;    // Which semaphore was taken/given (if applicable)
  unsigned int system_ceiling; // The SRP ceiling AT THE TIME of this event
  unsigned int preempt_level;  // Preemption level of the acting task
  TickType_t   deadline;       // Absolute deadline of the acting task (for EDF checks)
} TraceRecord_t;

void TRACE_record( //
  const TraceEvent_t event,
  TraceTaskType_t    task_type,
  const TMB_t *const task
);

void TRACE_print_buffer();
void TRACE_disable();

#endif // TRACER_H
