#include "cbs_tests.h"

// TODO: EDF test 5 needs updated logic for the admission (u=1)

#include "FreeRTOS.h" // IWYU pragma: keep
#include "cbs.h"
#include "edf_scheduler.h"
#include "helpers.h"
#include <stdio.h>

#define GENERATE_APERIODIC_TASK(name, ticks)                                                                           \
  BaseType_t CBS_task_##name(void) {                                                                                   \
    execute_for_ticks(ticks);                                                                                          \
    return pdTRUE;                                                                                                     \
  }

GENERATE_APERIODIC_TASK(400, 400)
GENERATE_APERIODIC_TASK(300, 300)

// Wrapper over periodic task
BaseType_t platform_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
) {
#if USE_EDF
  return EDF_create_periodic_task(task_function, task_name, completion_time, period, relative_deadline, TMB_handle);
#else
#error "No scheduler implementation defined in `create_periodic_task`"
  return pdFAIL;
#endif
};

; // ====================================
; // === Tests for Base Functionality ===
; // ====================================

void vTestRunner1() {
  const int CBS_SERVER_ID = 1;
  create_cbs_server(pdMS_TO_TICKS(300), pdMS_TO_TICKS(800), CBS_SERVER_ID);
  platform_create_periodic_task( //
    EDF_periodic_task,
    "Task P1",
    pdMS_TO_TICKS(400),
    pdMS_TO_TICKS(700),
    pdMS_TO_TICKS(700),
    NULL
  );

  vTaskDelay(pdMS_TO_TICKS(300));
  CBS_create_aperiodic_task(CBS_task_400, CBS_SERVER_ID);
  vTaskDelay(pdMS_TO_TICKS(1000));

  CBS_create_aperiodic_task(CBS_task_300, CBS_SERVER_ID);
  vTaskDelay(1100);
  vTaskDelete(NULL);
}

// Smoke test (textbook pg.190): 1 periodic task with 2 aperiodic tasks; 1 CBS server
TickType_t cbs_test_1() {
  xTaskCreate( //
    vTestRunner1,
    "test runner 1",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );

  const TickType_t TEST_DURATION = 2400; // TODO: current value is arbitrary choice
  return TEST_DURATION;
}