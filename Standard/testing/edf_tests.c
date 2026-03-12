#include "edf_tests.h"

// TODO: EDF test 5 needs updated logic for the admission (u=1)

// TODO: EDF test 9 is crashing without outputting any trace

// TODO: EDF test 10 is crashing without outputting any trace

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"

#include <stdio.h>

; // ====================================
; // === Tests for Base Functionality ===
; // ====================================

// Smoke Test for Periodic Tasks (relative deadline == period)
TickType_t edf_test_1() {
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 1, Task 1",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(600),
    pdMS_TO_TICKS(600),
    NULL
  );
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 1, Task 2",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(200),
    NULL
  );

  const TickType_t TEST_DURATION = 1200;
  return TEST_DURATION;
}

// Test 2: Mark's Deadline DNE Period Smoke Test
TickType_t edf_test_2() {
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 2, Task 1",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(600),
    pdMS_TO_TICKS(400),
    NULL
  );
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 2, Task 2",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(800),
    pdMS_TO_TICKS(500),
    NULL
  );
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 2, Task 3",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(300),
    pdMS_TO_TICKS(900),
    pdMS_TO_TICKS(700),
    NULL
  );

  const TickType_t TEST_DURATION = 2400;
  return TEST_DURATION;
}

; // ===============================
; // === Admission Control Tests ===
; // ===============================

// TEST3: 100 Tasks NON-ADMISSIBLE
TickType_t edf_test_3() {
  for (int i = 0; i < 100; i++) {
    char taskName[19];
    sprintf(taskName, "EDF Test 3, Task %d", i);
    EDF_create_periodic_task( //
      EDF_periodic_task,
      taskName,
      configMINIMAL_STACK_SIZE,
      pdMS_TO_TICKS(15),
      pdMS_TO_TICKS(1000),
      pdMS_TO_TICKS(500),
      NULL
    );
  }

  // Test duration is set to 0, since the point is to test the admission control
  const TickType_t TEST_DURATION = 0;
  return TEST_DURATION;
}

// TEST4: 100 Tasks ADMISSIBLE
TickType_t edf_test_4() {
  for (int i = 0; i < 100; i++) {
    // NB: This breaks without downstream copying of task
    char taskName[19];
    sprintf(taskName, "EDF Test 4, Task %d", i);
    EDF_create_periodic_task( //
      EDF_periodic_task,
      taskName,
      configMINIMAL_STACK_SIZE,
      pdMS_TO_TICKS(8),
      pdMS_TO_TICKS(1000),
      pdMS_TO_TICKS(1000),
      NULL
    );
  }

  // Test duration is set to 0, since the point is to test the admission control
  const TickType_t TEST_DURATION = 0;
  return TEST_DURATION;
}

// TEST 5: BARELY ADMISSIBLE BY UTILIZATION (10 tasks * 10ms = 100ms demand every 100ms)
// Total Utilization = 1.0 (100%)
TickType_t edf_test_5() {
  for (int i = 0; i < 10; i++) {
    char taskName[19];
    sprintf(taskName, "EDF Test 5, Task %d", i);
    EDF_create_periodic_task( //
      EDF_periodic_task,
      taskName,
      configMINIMAL_STACK_SIZE,
      pdMS_TO_TICKS(10),
      pdMS_TO_TICKS(100),
      pdMS_TO_TICKS(100),
      NULL
    );
  }

  // Test duration is set to 0, since the point is to test the admission control
  const TickType_t TEST_DURATION = 0;
  return TEST_DURATION;
}

// TEST 6: BARELY NON-ADMISSIBLE BY UTILIZATION (10 tasks * 11ms = 110ms demand every 100ms)
// Total Utilization = 1.1 (110%)
TickType_t edf_test_6() {
  for (int i = 0; i < 10; i++) {
    char taskName[19];
    sprintf(taskName, "EDF Test 6, Task %d", i);
    EDF_create_periodic_task( //
      EDF_periodic_task,
      taskName,
      configMINIMAL_STACK_SIZE,
      pdMS_TO_TICKS(11),
      pdMS_TO_TICKS(100),
      pdMS_TO_TICKS(100),
      NULL
    );
  }

  // Test duration is set to 0, since the point is to test the admission control
  const TickType_t TEST_DURATION = 0;
  return TEST_DURATION;
}

// TEST 7: BARELY ADMISSIBLE BY PROCESSOR DEMAND (both U and demand are below upper bounds)
TickType_t edf_test_7() {
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 7, Task 1",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(10),
    pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(50),
    NULL
  );
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 7, Task 2",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(40),
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(50),
    NULL
  );

  const TickType_t TEST_DURATION = 400;
  return TEST_DURATION;
}

// --- TEST 8: BARELY NON-ADMISSIBLE BY DEMAND (U is only 42% but demand > 1 at L = 50) ---
TickType_t edf_test_8() {
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 8, Task 1",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(11),
    pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(50),
    NULL
  );
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 8, Task 2",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(40),
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(50),
    NULL
  );

  // Test duration is set to 0, since the point is to test the admission control
  const TickType_t TEST_DURATION = 0;
  return TEST_DURATION;
}

; // ==========================================================
; // === Tests for Drop-in of Tasks while System is Running ===
; // ==========================================================

// TODO: Not sure if vTaskCreate calling xTaskCreatePeriodic, which calls vTaskCreate is a
//       good design

// TEST 9: Admissible Drop-in
void vTestRunner9() {
  // --- TEST A: Admissible Drop-in ---
  // Base Task: 160ms work, 800ms period

  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 9, Task 1",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(8 * 20),
    pdMS_TO_TICKS(8 * 100),
    pdMS_TO_TICKS(8 * 100),
    NULL
  );

  // Wait 5 cycles (500ms) to show stable execution
  vTaskDelay(pdMS_TO_TICKS(500));

  // Drop-in Task: 400ms work, 800ms period.
  // Total Demand: 560ms < 800ms. Should pass PDC.
  // TODO: show why total demand is not exceeded
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 9, Task 2",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(8 * 50),
    pdMS_TO_TICKS(8 * 100),
    pdMS_TO_TICKS(8 * 100),
    NULL
  );
  vTaskDelete(NULL);
}
TickType_t edf_test_9() {
  xTaskCreate( //
    vTestRunner9,
    "EDF Test 9, Test Runner",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,
    NULL
  );

  const TickType_t TEST_DURATION = 1200;
  return TEST_DURATION;
}

// TEST 10: Inadmissible Drop-in
void vTestRunner10() {
  // --- TEST B: Inadmissible Drop-in ---
  // Base Task: 20ms work, 100ms period (U=0.2)
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 10, Task 1",
    configMINIMAL_STACK_SIZE,
    20,
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(100),
    NULL
  );

  vTaskDelay(pdMS_TO_TICKS(500));

  // Drop-in Task: 90ms work, 200ms period (U=0.45)
  // At L=100, Demand = 20 + 90 = 110ms. PDC Violation!
  BaseType_t result = EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 10, Task 2",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(90),
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(100),
    NULL
  );
  vTaskDelete(NULL);
}
TickType_t edf_test_10() {
  xTaskCreate( //
    vTestRunner10,
    "EDF Test 10, Test Runner",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,
    NULL
  );

  // Test duration is set to 0, since the point is to test the admission control
  const TickType_t TEST_DURATION = 0;
  return TEST_DURATION;
}

; // =================================
; // === Tests for Missed Deadline ===
; // =================================

// TEST 11: Missed Deadline (Total Utilization: 105%)
TickType_t edf_test_11() {
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 11, Task 1",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(120),
    pdMS_TO_TICKS(50),
    NULL
  );
  EDF_create_periodic_task( //
    EDF_periodic_task,
    "EDF Test 11, Task 2",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(130),
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(200),
    NULL
  );

  const TickType_t TEST_DURATION = 250;
  return TEST_DURATION;
}
