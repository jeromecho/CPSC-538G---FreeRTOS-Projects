#include "tracer.h"

#include "pico/time.h"

#if USE_SRP
#include "srp.h"
#endif

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static TraceRecord_t trace_buffer[configNUMBER_OF_CORES][MAX_TRACE_RECORDS];
static size_t        trace_count[configNUMBER_OF_CORES] = {0};

static bool tracing_enabled = true;

static size_t TRACE_find_duplicate_background_event_slot(
  const TickType_t trace_tick, const TraceEvent_t event, const TraceTaskType_t task_type, const uint8_t reported_core_id
) {
  if (task_type != TRACE_TASK_IDLE && task_type != TRACE_TASK_SYSTEM) {
    return SIZE_MAX;
  }

  for (size_t i = trace_count[reported_core_id]; i > 0; --i) {
    const TraceRecord_t *const existing = &trace_buffer[reported_core_id][i - 1];

    if (existing->FreeRTOS_tick != trace_tick) {
      break;
    }

    if (existing->event.type == event.type && existing->task_type == task_type) {
      return i - 1;
    }
  }

  return SIZE_MAX;
}

// TODO: This function should maybe differ when SRP is enabled vs when it is not, since the trace event structure is a
// bit different for SRP vs EDF. For now, just include all SRP-related fields in the trace event, but they will be set
// to 0 when SRP is not enabled.
/// @brief Records a trace, so debugging is simpler even without a logic analyzer
void TRACE_record( //
  const TraceEvent_t event,
  TraceTaskType_t    task_type,
  const TMB_t *const task,
  const bool         in_ISR
) {
  if (!tracing_enabled) {
    return;
  }

  if (!in_ISR) {
    taskENTER_CRITICAL();
  }

  const uint8_t emitting_core_id = (uint8_t)portGET_CORE_ID();
  uint8_t       reported_core_id = emitting_core_id;
#if USE_MP
  if (task != NULL) {
    reported_core_id = task->assigned_core;
  }
#endif

  if (trace_count[reported_core_id] >= MAX_TRACE_RECORDS) {
    if (!in_ISR) {
      taskEXIT_CRITICAL();
    }
    return;
  }

  if (task_type == TRACE_TASK_EITHER && task != NULL) {
    task_type = (task->type == TASK_PERIODIC) ? TRACE_TASK_PERIODIC : TRACE_TASK_APERIODIC;
  }

  uint8_t     task_id           = UINT8_MAX;
  uint32_t    task_uid          = UINT32_MAX;
  TickType_t  deadline          = portMAX_DELAY;
  UBaseType_t freeRTOS_priority = UINT_MAX;
  eTaskState  task_state        = eInvalid;
  uint8_t     debug_code        = UINT8_MAX;

  // SRP-related (not updated when SRP is disabled)
  unsigned int system_ceiling = UINT_MAX;
  unsigned int preempt_level  = UINT_MAX;

  if (task != NULL) {
    task_id  = task->id;
    task_uid = task->trace_uid;
    deadline = task->absolute_deadline;
    if (!in_ISR && task->handle != NULL) {
      freeRTOS_priority = uxTaskPriorityGet(task->handle);
      task_state        = eTaskGetState(task->handle);
    }
  }

#if USE_SRP
  system_ceiling = SRP_get_system_ceiling();
  if (task != NULL) {
    preempt_level = task->preemption_level;
  }
#endif // USE_SRP

  if (task_type == TRACE_TASK_IDLE || task_type == TRACE_TASK_SYSTEM) {
    // Distinguish per-core idle tasks in SMP traces.
    task_id = emitting_core_id;
    task_uid = emitting_core_id;
  }

  if (event.type == TRACE_DEBUG_MARKER) {
    debug_code = event.data.debug_code;
  }

  const TickType_t trace_tick = in_ISR ? xTaskGetTickCountFromISR() : xTaskGetTickCount();
  const size_t     duplicate_slot =
    TRACE_find_duplicate_background_event_slot(trace_tick, event, task_type, reported_core_id);
  const size_t slot = (duplicate_slot != SIZE_MAX) ? duplicate_slot : trace_count[reported_core_id];

  trace_buffer[reported_core_id][slot] = (TraceRecord_t){
    .FreeRTOS_tick  = trace_tick,
    .time           = get_absolute_time(),
    .core_id        = reported_core_id,
    .core_seq       = (uint32_t)slot,
    .event          = event,
    .task_type      = task_type,
    .task_id        = task_id,
    .task_uid       = task_uid,
    .deadline       = deadline,
    .priority       = freeRTOS_priority,
    .task_state     = task_state,
    .debug_code     = debug_code,
    .system_ceiling = system_ceiling,
    .preempt_level  = preempt_level,
  };

  if (duplicate_slot == SIZE_MAX) {
    trace_count[reported_core_id]++;
  }
  if (!in_ISR) {
    taskEXIT_CRITICAL();
  }
}

/// @brief Prints all recorded traces to the host computer
void TRACE_print_buffer() {
  TRACE_disable();

#if configNUMBER_OF_CORES > 2
#error "Only two cores are supported"
#endif

  size_t trace_total_count = 0;
  for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
    trace_total_count += trace_count[core];
  }

  printf("\n--- TEST COMPLETE ---\n");
  printf("Traces captured: %u\n", trace_total_count);
  printf("TIMESTAMP,EVENT,ABS_TIME,CORE,CORE_SEQ,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,DEBUG_CODE,CEILING,"
         "PREEMPT_LVL,DEADLINE,TASK_UID\n");

  size_t head[configNUMBER_OF_CORES] = {0};

  for (size_t printed = 0; printed < trace_total_count; printed++) {
    size_t best_core = SIZE_MAX;

    for (size_t core = 0; core < configNUMBER_OF_CORES; core++) {
      if (head[core] >= trace_count[core]) {
        continue;
      }

      if (best_core == SIZE_MAX) {
        best_core = core;
        continue;
      }

      const TraceRecord_t *const candidate = &trace_buffer[core][head[core]];
      const TraceRecord_t *const best      = &trace_buffer[best_core][head[best_core]];

      const uint64_t candidate_us = to_us_since_boot(candidate->time);
      const uint64_t best_us      = to_us_since_boot(best->time);

      if (candidate_us < best_us ||
          (candidate_us == best_us && (candidate->core_id < best->core_id || (candidate->core_id == best->core_id &&
                                                                              candidate->core_seq < best->core_seq)))) {
        best_core = core;
      }
    }

    if (best_core == SIZE_MAX) {
      break;
    }

    const TraceRecord_t *const r = &trace_buffer[best_core][head[best_core]];
    head[best_core]++;

    uint8_t resource_id = UINT8_MAX;
    if (r->event.type == TRACE_SEMAPHORE_TAKE || r->event.type == TRACE_SEMAPHORE_GIVE) {
      resource_id = r->event.data.semaphore_index;
    }

    uint8_t task_id = r->task_id;
    if (r->event.type == TRACE_ADMISSION_FAILED) {
      task_id = r->event.data.task_index;
    }

    printf(
      "%u,%d,%llu,%u,%lu,%d,%u,%u,%d,%u,%u,%u,%u,%u,%u\n",
      r->FreeRTOS_tick,
      (int)r->event.type,
      to_us_since_boot(r->time),
      r->core_id,
      (unsigned long)r->core_seq,
      r->task_type,
      (unsigned int)task_id,
      (unsigned int)r->priority,
      (int)r->task_state,
      resource_id,
      r->debug_code,
      r->system_ceiling,
      r->preempt_level,
      r->deadline,
      r->task_uid
    );
  }

  printf("--- END OF TRACE ---\n");
}

/// @brief Turns off the tracing functionality, preventing any additional traces from being recorded
void TRACE_disable(void) { tracing_enabled = false; }
