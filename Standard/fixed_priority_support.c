#include "ProjectConfig.h"

#if !USE_EDF

#include "fixed_priority_support.h"

#include "FreeRTOS_include.h"

#include "pico/time.h"
#include "task.h"

#include <stdint.h>
#include <stdio.h>

static StaticTask_t idle_task_tcb[configNUMBER_OF_CORES];
static StackType_t  idle_task_stack[configNUMBER_OF_CORES][configMINIMAL_STACK_SIZE];

static StaticTask_t timer_task_tcb;
static StackType_t  timer_task_stack[configTIMER_TASK_STACK_DEPTH];

typedef struct {
  TickType_t  tick;
  uint8_t     event;
  uint8_t     task_type;
  uint8_t     task_id;
  UBaseType_t priority;
  uint32_t    task_uid;
  uint64_t    abs_time_us;
} FPTraceRecord_t;

static FPTraceRecord_t fp_trace_buffer[MAX_TRACE_RECORDS];
static size_t          fp_trace_count = 0;

static TaskHandle_t fp_high_task_handle = NULL;
static TaskHandle_t fp_low_task_handle  = NULL;

enum {
  FP_TRACE_RELEASE = 0,
  FP_TRACE_SWITCH_IN = 1,
  FP_TRACE_SWITCH_OUT = 2,
  FP_TRACE_DONE = 3,
};

static void FP_record_event(const uint8_t event_type, const TaskHandle_t task_handle) {
  if (fp_trace_count >= MAX_TRACE_RECORDS) {
    return;
  }

  uint8_t  task_type = 3; // TRACE_TASK_SYSTEM
  uint8_t  task_id   = UINT8_MAX;
  uint32_t task_uid  = UINT32_MAX;

  if (task_handle == fp_high_task_handle) {
    task_type = 1; // TRACE_TASK_PERIODIC
    task_id   = 0;
    task_uid  = 0;
  } else if (task_handle == fp_low_task_handle) {
    task_type = 1; // TRACE_TASK_PERIODIC
    task_id   = 1;
    task_uid  = 1;
  }

  fp_trace_buffer[fp_trace_count++] = (FPTraceRecord_t){
    .tick        = xTaskGetTickCount(),
    .event       = event_type,
    .task_type   = task_type,
    .task_id     = task_id,
    .priority    = (task_handle != NULL) ? uxTaskPriorityGet(task_handle) : 0,
    .task_uid    = task_uid,
    .abs_time_us = to_us_since_boot(get_absolute_time()),
  };
}

void FP_trace_reset(void) {
  taskENTER_CRITICAL();
  fp_trace_count = 0;
  taskEXIT_CRITICAL();
}

void FP_trace_register_tasks(TaskHandle_t high_task, TaskHandle_t low_task) {
  taskENTER_CRITICAL();
  fp_high_task_handle = high_task;
  fp_low_task_handle  = low_task;
  taskEXIT_CRITICAL();
}

void FP_trace_print_buffer(void) {
  printf("\n--- TEST COMPLETE ---\n");
  printf("Traces captured: %u\n", (unsigned int)fp_trace_count);
  printf("TIMESTAMP,EVENT,ABS_TIME,CORE,CORE_SEQ,TASK_TYPE,TASK_ID,PRIORITY,TASK_STATE,RESOURCE,DEBUG_CODE,CEILING,"
         "PREEMPT_LVL,DEADLINE,TASK_UID\n");

  for (size_t i = 0; i < fp_trace_count; i++) {
    const FPTraceRecord_t *const r = &fp_trace_buffer[i];
    printf(
      "%u,%u,%llu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
      (unsigned int)r->tick,
      (unsigned int)r->event,
      (unsigned long long)r->abs_time_us,
      0u,
      (unsigned int)i,
      (unsigned int)r->task_type,
      (unsigned int)r->task_id,
      (unsigned int)r->priority,
      0u,
      255u,
      255u,
      255u,
      255u,
      0u,
      (unsigned int)r->task_uid
    );
  }

  printf("--- END OF TRACE ---\n");
}

void task_switched_in(void) {
  FP_record_event(FP_TRACE_SWITCH_IN, xTaskGetCurrentTaskHandle());
}

void task_switched_out(void) {
  FP_record_event(FP_TRACE_SWITCH_OUT, xTaskGetCurrentTaskHandle());
}

void starting_scheduler(void *xIdleTaskHandles) { (void)xIdleTaskHandles; }

void vApplicationTickHook(void) {}

void vApplicationGetIdleTaskMemory(
  StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize
) {
  *ppxIdleTaskTCBBuffer   = &idle_task_tcb[0];
  *ppxIdleTaskStackBuffer = idle_task_stack[0];
  *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(
  StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize
) {
  *ppxTimerTaskTCBBuffer   = &timer_task_tcb;
  *ppxTimerTaskStackBuffer = timer_task_stack;
  *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

#endif // !USE_EDF
