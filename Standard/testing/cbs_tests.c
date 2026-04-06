#include "cbs_tests.h"

// TODO: EDF test 5 needs updated logic for the admission (u=1)

#include "FreeRTOS.h" // IWYU pragma: keep
#include "cbs.h"
#include "edf_scheduler.h"
#include "helpers.h"
#include <stdio.h>

// ======= FUNCTION MACROS ========
// ================================

#define GENERATE_APERIODIC_TASK(name, ms)                                                                              \
  BaseType_t CBS_task_##name(void) {                                                                                   \
    execute_for_ticks(pdMS_TO_TICKS(ms));                                                                              \
    return pdTRUE;                                                                                                     \
  }

GENERATE_APERIODIC_TASK(400, 400)
GENERATE_APERIODIC_TASK(300, 300)
GENERATE_APERIODIC_TASK(4, 4)
GENERATE_APERIODIC_TASK(3, 3)

// ========== CONSTANTS ===========
// ================================

const int CBS_SERVER1_ID = 1;

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
  vTaskDelay(pdMS_TO_TICKS(3));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  vTaskDelay(pdMS_TO_TICKS(10));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID);
  vTaskDelay(pdMS_TO_TICKS(11));
  vTaskDelete(NULL);
}

void vTestRunner2() {
  const int CBS_SERVER_ID = 1;
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER_ID);
  for (size_t i = 0; i < CBS_QUEUE_CAPACITY; i++) {
    CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER_ID);
  }
  /*
   */
  vTaskDelay(pdMS_TO_TICKS(50));
  vTaskDelete(NULL);
}

// Multiple tasks queueing up on 1 CBS server
TickType_t cbs_test_2() {
  xTaskCreate( //
    vTestRunner2,
    "test runner 2",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(50);
}

// Smoke test (textbook pg.190): 1 periodic task with 2 aperiodic tasks; 1 CBS server
TickType_t cbs_test_1() {
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  xTaskCreate( //
    vTestRunner1,
    "test runner 1",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );

  return pdMS_TO_TICKS(21);
}