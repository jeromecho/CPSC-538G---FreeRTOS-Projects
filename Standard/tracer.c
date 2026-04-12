#include "tracer.h"

#include "pico/time.h"

#if USE_SRP
#include "srp.h"
#endif

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static TraceRecord_t trace_buffer[MAX_TRACE_RECORDS];
static size_t        trace_count = 0;

static bool tracing_enabled = true;

// TODO: This function should maybe differ when SRP is enabled vs when it is not, since the trace event structure is a
// bit different for SRP vs EDF. For now, just include all SRP-related fields in the trace event, but they will be set
// to 0 when SRP is not enabled.
/// @brief Records a trace, so debugging is simpler even without a logic analyzer
void TRACE_record( //
  const TraceEvent_t event,
  TraceTaskType_t    task_type,
  const TMB_t *const task
) {
  if (!tracing_enabled || trace_count >= MAX_TRACE_RECORDS) {
    return;
  }

  taskENTER_CRITICAL();

  if (task_type == TRACE_TASK_EITHER) {
    task_type = (task->type == TASK_PERIODIC) ? TRACE_TASK_PERIODIC : TRACE_TASK_APERIODIC;
  }

  size_t      task_id    = UINT8_MAX;
  TickType_t  deadline   = portMAX_DELAY;
  UBaseType_t priority   = UINT_MAX;
  eTaskState  task_state = eInvalid;

  // SRP-related (not updated when SRP is disabled)
  unsigned int system_ceiling = UINT_MAX;
  unsigned int preempt_level  = UINT_MAX;

  if (task != NULL) {
    task_id  = task->id;
    deadline = task->absolute_deadline;
    if (task->handle != NULL) {
      priority   = uxTaskPriorityGet(task->handle);
      task_state = eTaskGetState(task->handle);
    }
  }

#if USE_SRP
  system_ceiling = SRP_get_system_ceiling();
  if (task != NULL) {
    preempt_level = task->preemption_level;
  }
#endif // USE_SRP

  trace_buffer[trace_count] = (TraceRecord_t){
    .FreeRTOS_tick  = xTaskGetTickCount(),
    .time           = get_absolute_time(),
    .event          = event,
    .task_type      = task_type,
    .task_id        = task_id,
    .deadline       = deadline,
    .priority       = priority,
    .task_state     = task_state,
    .system_ceiling = system_ceiling,
    .preempt_level  = preempt_level,
  };

  trace_count++;
  taskEXIT_CRITICAL();
}

/// @brief Prints all recorded traces to the host computer
void TRACE_print_buffer() {
  TRACE_disable();

  printf("\n--- TEST COMPLETE ---\n");
  printf("Traces captured: %u\n", trace_count);
  printf("TIMESTAMP,EVENT,ABS_TIME,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,CEILING,PREEMPT_LVL,DEADLINE\n");

  for (size_t i = 0; i < trace_count; i++) {
    const TraceRecord_t *const r = &trace_buffer[i];

    uint8_t resource_id = UINT8_MAX;
    if (r->event.type == TRACE_SEMAPHORE_TAKE || r->event.type == TRACE_SEMAPHORE_GIVE) {
      resource_id = r->event.data.semaphore_index;
    }

    size_t task_id = r->task_id;
    if (r->event.type == TRACE_ADMISSION_FAILED) {
      task_id = r->event.data.task_index;
    }

    printf(
      "%u,%d,%llu,%d,%u,%u,%d,%u,%u,%u,%u\n",
      r->FreeRTOS_tick,
      (int)r->event.type,
      to_us_since_boot(r->time),
      r->task_type,
      task_id,
      (unsigned int)r->priority,
      (int)r->task_state,
      resource_id,
      r->system_ceiling,
      r->preempt_level,
      r->deadline
    );
  }

  printf("--- END OF TRACE ---\n");
}

/// @brief Turns off the tracing functionality, preventing any additional traces from being recorded
void TRACE_disable(void) { tracing_enabled = false; }
