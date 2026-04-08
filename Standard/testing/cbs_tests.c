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
GENERATE_APERIODIC_TASK(2, 2)
GENERATE_APERIODIC_TASK(1, 1)

// ========== CONSTANTS ===========
// ================================

const int CBS_SERVER1_ID = 1;
const int CBS_SERVER2_ID = 2;
const int CBS_SERVER3_ID = 3;

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
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  for (size_t i = 0; i < CBS_QUEUE_CAPACITY; i++) {
    CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  }
  vTaskDelay(pdMS_TO_TICKS(50));
  vTaskDelete(NULL);
}

void vTestRunner4() {
  create_cbs_server(pdMS_TO_TICKS(2), pdMS_TO_TICKS(10), CBS_SERVER1_ID);
  platform_create_periodic_task(EDF_periodic_task, "P1", pdMS_TO_TICKS(2), pdMS_TO_TICKS(5), pdMS_TO_TICKS(4), NULL);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  vTaskDelete(NULL);
}

void vTestRunner5() {
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  vTaskDelete(NULL);
}

void vTestRunner6() {
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER2_ID);
  vTaskDelay(pdMS_TO_TICKS(20));

  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER2_ID);
  vTaskDelay(pdMS_TO_TICKS(20));

  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER2_ID);

  vTaskDelete(NULL);
}

void (*vTestRunner7)() = vTestRunner6;
void (*vTestRunner8)() = vTestRunner6;
void (*vTestRunner9)() = vTestRunner6;

void vTestRunner10() {
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER2_ID);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER3_ID);
  vTaskDelay(pdMS_TO_TICKS(20));

  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER2_ID);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER3_ID);
  vTaskDelay(pdMS_TO_TICKS(20));

  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER1_ID);
  vTaskDelay(pdMS_TO_TICKS(10));

  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER2_ID);
  vTaskDelay(pdMS_TO_TICKS(10));

  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER2_ID);
  vTaskDelete(NULL);
}

void (*vTestRunner11)() = vTestRunner6;
void (*vTestRunner12)() = vTestRunner10;

void vTestRunner13() {
  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID);
  vTaskDelay(pdMS_TO_TICKS(8));
  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID);
  vTaskDelay(pdMS_TO_TICKS(8));
  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID);
  vTaskDelete(NULL);
}

void vTestRunner14() {
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  vTaskDelay(pdMS_TO_TICKS(1));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  vTaskDelay(pdMS_TO_TICKS(1));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  vTaskDelay(pdMS_TO_TICKS(1));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  vTaskDelay(pdMS_TO_TICKS(1));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
  vTaskDelete(NULL);
}

void(*vTestRunner15) = vTestRunner1;
void(*vTestRunner16) = vTestRunner1;
void(*vTestRunner17) = vTestRunner1;

// Smoke test #1 (textbook pg.190): 1 periodic task with 2 aperiodic tasks; 1 CBS server
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

// Single aperiodic task running on CBS server
TickType_t cbs_test_2() {
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID);
}

// Multiple tasks queueing up to max capacity on 1 CBS server
TickType_t cbs_test_3() {
  xTaskCreate(
    vTestRunner2,
    "test runner 2",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(50);
}

// Smoke Test #2: Different setup with 1 periodic task and 1 CBS server
TickType_t cbs_test_4() {
  xTaskCreate(
    vTestRunner4,
    "test runner 4",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(30);
}

// Smoke Test #3: Multiple Periodic Tasks Running Alongside Single CBS Server
TickType_t cbs_test_5() {
  create_cbs_server(pdMS_TO_TICKS(2), pdMS_TO_TICKS(10), CBS_SERVER1_ID);
  platform_create_periodic_task(EDF_periodic_task, "P1", pdMS_TO_TICKS(2), pdMS_TO_TICKS(4), pdMS_TO_TICKS(4), NULL);
  platform_create_periodic_task(EDF_periodic_task, "P2", pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), pdMS_TO_TICKS(8), NULL);
  xTaskCreate(
    vTestRunner5,
    "test runner 5",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(50);
}

// Smoke Test #4: Multiple Periodic Tasks Running Alongside 2 symmetric CBS servers
TickType_t cbs_test_6() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(2), pdMS_TO_TICKS(6), pdMS_TO_TICKS(6), NULL
  );
  platform_create_periodic_task(
    EDF_periodic_task, "Task P2", pdMS_TO_TICKS(1), pdMS_TO_TICKS(3), pdMS_TO_TICKS(3), NULL
  );
  xTaskCreate(
    vTestRunner6,
    "test runner 6",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(100);
}

// Smoke Test #4: Multiple Periodic Tasks Running Alongside 2 asymmetric CBS servers
TickType_t cbs_test_7() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(4), pdMS_TO_TICKS(8), CBS_SERVER2_ID);

  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(2), pdMS_TO_TICKS(6), pdMS_TO_TICKS(6), NULL
  );
  platform_create_periodic_task(
    EDF_periodic_task, "Task P2", pdMS_TO_TICKS(1), pdMS_TO_TICKS(3), pdMS_TO_TICKS(3), NULL
  );
  xTaskCreate(
    vTestRunner7,
    "test runner 7",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(100);
}

// Multiple (2) symmetric CBS servers in isolation
TickType_t cbs_test_8() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  xTaskCreate(
    vTestRunner8,
    "test runner 8",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(60);
}

// Multiple (2) asymmetric CBS servers in isolation
TickType_t cbs_test_9() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(4), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  xTaskCreate(
    vTestRunner9,
    "test runner 9",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(60);
}

// Multiple (3) aymmetric CBS servers in isolation
TickType_t cbs_test_10() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(2), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER3_ID);
  xTaskCreate(
    vTestRunner10,
    "test runner 10",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(80);
}

// Multiple (2) CBS servers running alongside 1 periodic task
TickType_t cbs_test_11() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(4), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(2), pdMS_TO_TICKS(6), pdMS_TO_TICKS(6), NULL
  );
  xTaskCreate(
    vTestRunner11,
    "test runner 11",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(100);
}

// Multiple (3) CBS servers running alongside 1 periodic task
TickType_t cbs_test_12() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(2), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER3_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(2), pdMS_TO_TICKS(6), pdMS_TO_TICKS(6), NULL
  );
  xTaskCreate(
    vTestRunner12,
    "test runner 12",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(100);
}

// 1 CBS Server, 1 periodic task. Bandwidth is high but load of aperiodic tasks is low.
// No deadline miss.
TickType_t cbs_test_13() {
  create_cbs_server(pdMS_TO_TICKS(8), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  xTaskCreate( //
    vTestRunner13,
    "test runner 13",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(21);
}

// 1 CBS Server, 1 periodic task. Bandwidth is high and load of aperiodic tasks is high.
// Deadline miss.
TickType_t cbs_test_14() {
  create_cbs_server(pdMS_TO_TICKS(8), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  xTaskCreate( //
    vTestRunner14,
    "test runner 14",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(21);
}

// Textbook smoke test but with lower server bandwidth
TickType_t cbs_test_15() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  xTaskCreate( //
    vTestRunner15,
    "test runner 15",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(21);
}

// Textbook smoke test but with higher (but not 100%) server bandwidth
TickType_t cbs_test_16() {
  create_cbs_server(pdMS_TO_TICKS(7), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  xTaskCreate( //
    vTestRunner16,
    "test runner 16",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
  return pdMS_TO_TICKS(21);
}

// Textbook smoke test but with 100% server bandwidth
TickType_t cbs_test_17() {
  create_cbs_server(pdMS_TO_TICKS(8), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  xTaskCreate( //
    vTestRunner17,
    "test runner 17",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
}
