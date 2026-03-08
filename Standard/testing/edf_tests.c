#include "edf_tests.h"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"

#include <stdio.h>

/**
 * Tests for Missed Deadline (Note: Turn off/Comment Out Admission Control to Allow Easy Deadline
 * Miss)
 */
// TEST 11: Missed Deadline (Total Utilization: 105%)
void edf_test_11() {
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Task_A",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(40),
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(100),
    NULL
  );
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Task_B",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(130),
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(200),
    NULL
  );
}

/**
 * Tests for Drop-in of Tasks while System is Running
 */
// TODO: Not sure if vTaskCreate calling xTaskCreatePeriodic, which calls vTaskCreate is a
//       good design
// TODO: magic numbers for priority of below function calls
// TEST 10: Inadmissible Drop-in
void vTestRunner10() {
  // --- TEST B: Inadmissible Drop-in ---
  // Base Task: 20ms work, 100ms period (U=0.2)
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Base_B",
    configMINIMAL_STACK_SIZE,
    20,
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(100),
    NULL
  );

  vTaskDelay(pdMS_TO_TICKS(500));

  // Drop-in Task: 90ms work, 200ms period (U=0.45)
  // At L=100, Demand = 20 + 90 = 110ms. PDC Violation!
  BaseType_t result = xTaskCreatePeriodic( //
    vPeriodicTask,
    "Drop_B",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(90),
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(100),
    NULL
  );
  vTaskDelete(NULL);
}
void edf_test_10() {
  xTaskCreate( //
    vTestRunner10,
    "test runner 10",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
}

// TEST 9: Admissible Drop-in
void vTestRunner9() {
  // --- TEST A: Admissible Drop-in ---
  // Base Task: 160ms work, 800ms period
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Base_A",
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
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Drop_A",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(8 * 50),
    pdMS_TO_TICKS(8 * 100),
    pdMS_TO_TICKS(8 * 100),
    NULL
  );
  vTaskDelete(NULL);
}
void edf_test_9() {
  xTaskCreate( //
    vTestRunner9,
    "test runner 9",
    configMINIMAL_STACK_SIZE,
    NULL, // Task parameter
    2,    // Task priority
    NULL
  );
}

/**
 * Admission Control Tests
 */
// --- TEST 8: BARELY NON-ADMISSIBLE BY DEMAND (U is only 42% but demand > 1 at L = 50) ---
void edf_test_8() {
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Fail_A",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(11),
    pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(50),
    NULL
  );
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Fail_B",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(40),
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(50),
    NULL
  );
}

// TEST 7: BARELY ADMISSIBLE BY PROCESSOR DEMAND (both U and demand are below upper bounds)
void edf_test_7() {
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Adm_A",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(10),
    pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(50),
    NULL
  );
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Adm_B",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(40),
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(50),
    NULL
  );
}

// TEST 6: BARELY NON-ADMISSIBLE BY UTILIZATION (10 tasks * 11ms = 110ms demand every 100ms)
// Total Utilization = 1.1 (110%)
void edf_test_6() {
  for (int i = 0; i < 10; i++) {
    char taskName[16];
    sprintf(taskName, "Fail_%d", i);
    xTaskCreatePeriodic( //
      vPeriodicTask,
      taskName,
      configMINIMAL_STACK_SIZE,
      pdMS_TO_TICKS(11),
      pdMS_TO_TICKS(100),
      pdMS_TO_TICKS(100),
      NULL
    );
  }
}

// TEST 5: BARELY ADMISSIBLE BY UTILIZATION (10 tasks * 10ms = 100ms demand every 100ms)
// Total Utilization = 1.0 (100%)
void edf_test_5() {
  for (int i = 0; i < 10; i++) {
    char taskName[16];
    sprintf(taskName, "Adm_%d", i);
    xTaskCreatePeriodic( //
      vPeriodicTask,
      taskName,
      configMINIMAL_STACK_SIZE,
      pdMS_TO_TICKS(10),
      pdMS_TO_TICKS(100),
      pdMS_TO_TICKS(100),
      NULL
    );
  }
}

// TEST4: 100 Tasks ADMISSIBLE
void edf_test_4() {
  for (int i = 0; i < 100; i++) {
    // NB: This breaks without downstream copying of task
    char taskName[16];
    sprintf(taskName, "test %d", i);
    xTaskCreatePeriodic( //
      vPeriodicTask,
      taskName,
      configMINIMAL_STACK_SIZE,
      pdMS_TO_TICKS(8),
      pdMS_TO_TICKS(1000),
      pdMS_TO_TICKS(1000),
      NULL
    );
  }
}

// TEST3: 100 Tasks NON-ADMISSIBLE
void edf_test_3() {
  for (int i = 0; i < 100; i++) {
    char taskName[16];
    sprintf(taskName, "test %d", i);
    xTaskCreatePeriodic( //
      vPeriodicTask,
      taskName,
      configMINIMAL_STACK_SIZE,
      pdMS_TO_TICKS(15),
      pdMS_TO_TICKS(1000),
      pdMS_TO_TICKS(500),
      NULL
    );
  }
}

/**
 * Tests for Base Functionality
 */

// Test 2: Mark's Deadline DNE Period Smoke Test
void edf_test_2() {
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Periodic Task 1",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(600),
    pdMS_TO_TICKS(400),
    NULL
  );
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Periodic Task 2",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(800),
    pdMS_TO_TICKS(500),
    NULL
  );
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Periodic Task 3",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(300),
    pdMS_TO_TICKS(900),
    pdMS_TO_TICKS(700),
    NULL
  );
}

// Smoke Test for Periodic Tasks (relative deadline == period)
void edf_test_1() {
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Periodic Task 1",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(600),
    pdMS_TO_TICKS(600),
    NULL
  );
  xTaskCreatePeriodic( //
    vPeriodicTask,
    "Periodic Task 2",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(200),
    NULL
  );
}
