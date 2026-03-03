#include "edf_tests.h"

#include "FreeRTOS.h"
#include "edf_scheduler.h"

#include <stdio.h>

/**
 * Tests for Missed Deadline (Note: Turn off/Comment Out Admission Control to Allow Easy Deadline
 * Miss)
 */
// TEST 11: Missed Deadline (Total Utilization: 105%)
// xTaskCreatePeriodic(
//   vPeriodicTask, "Task_A", 512, (void *)40, pdMS_TO_TICKS(100), pdMS_TO_TICKS(100), NULL
// );
// xTaskCreatePeriodic(
//   vPeriodicTask, "Task_B", 512, (void *)130, pdMS_TO_TICKS(200), pdMS_TO_TICKS(200), NULL
// );
void edf_test_11() {
  xTaskCreatePeriodic(
    vPeriodicTask,             // Task function
    "Task_A",                  // Task name
    configMINIMAL_STACK_SIZE,  // Stack depth
    (void *)pdMS_TO_TICKS(40), // Completion time
    pdMS_TO_TICKS(100),        // Period
    pdMS_TO_TICKS(100),        // Relative Deadline
    NULL                       // Task handle
  );
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Task_B",                   // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(130), // Completion time
    pdMS_TO_TICKS(200),         // Period
    pdMS_TO_TICKS(200),         // Relative Deadline
    NULL                        // Task handle
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
  xTaskCreatePeriodic(
    vPeriodicTask,            // Task function
    "Base_B",                 // Task name
    configMINIMAL_STACK_SIZE, // Stack depth
    (void *)20,               // Completion time
    pdMS_TO_TICKS(100),       // Period
    pdMS_TO_TICKS(100),       // Relative Deadline
    NULL                      // Task handle
  );

  vTaskDelay(pdMS_TO_TICKS(500));

  // Drop-in Task: 90ms work, 200ms period (U=0.45)
  // At L=100, Demand = 20 + 90 = 110ms. PDC Violation!
  BaseType_t result = xTaskCreatePeriodic(
    vPeriodicTask,            // Task function
    "Drop_B",                 // Task name
    configMINIMAL_STACK_SIZE, // Stack depth
    (void *)90,               // Completion time
    pdMS_TO_TICKS(200),       // Period
    pdMS_TO_TICKS(100),       // Relative Deadline
    NULL                      // Task handle
  );
  vTaskDelete(NULL);
}
void edf_test_10() {
  xTaskCreate(
    vTestRunner10,            // Task function
    "test runner 10",         // Task name
    configMINIMAL_STACK_SIZE, // Stack depth
    NULL,                     // Task parameter
    2,                        // Task priority
    NULL                      // Task handle
  );
}

// TEST 9: Admissible Drop-in
void vTestRunner9() {
  // --- TEST A: Admissible Drop-in ---
  // Base Task: 160ms work, 800ms period
  xTaskCreatePeriodic(
    vPeriodicTask,            // Task function
    "Base_A",                 // Task name
    configMINIMAL_STACK_SIZE, // Stack depth
    (void *)(8 * 20),         // Completion time
    pdMS_TO_TICKS(8 * 100),   // Period
    pdMS_TO_TICKS(8 * 100),   // Relative Deadline
    NULL                      // Task handle
  );

  // Wait 5 cycles (500ms) to show stable execution
  vTaskDelay(pdMS_TO_TICKS(500));

  // Drop-in Task: 400ms work, 800ms period.
  // Total Demand: 560ms < 800ms. Should pass PDC.
  // TODO: show why total demand is not exceeded
  xTaskCreatePeriodic(
    vPeriodicTask, "Drop_A", configMINIMAL_STACK_SIZE, (void *)(8 * 50), pdMS_TO_TICKS(8 * 100),
    pdMS_TO_TICKS(8 * 100), NULL
  );
  vTaskDelete(NULL);
}
void edf_test_9() {
  xTaskCreate(
    vTestRunner9,             // Task function
    "test runner 9",          // Task name
    configMINIMAL_STACK_SIZE, // Stack depth
    NULL,                     // Task parameter
    2,                        // Task priority
    NULL                      // Task handle
  );
}

/**
 * Admission Control Tests
 */
// --- TEST 8: BARELY NON-ADMISSIBLE BY DEMAND (U is only 42% but demand > 1 at L = 50) ---
void edf_test_8() {
  xTaskCreatePeriodic(
    vPeriodicTask,             // Task function
    "Fail_A",                  // Task name
    configMINIMAL_STACK_SIZE,  // Stack depth
    (void *)pdMS_TO_TICKS(11), // Completion time
    pdMS_TO_TICKS(50),         // Period
    pdMS_TO_TICKS(50),         // Relative Deadline
    NULL                       // Task handle
  );
  xTaskCreatePeriodic(
    vPeriodicTask,             // Task function
    "Fail_B",                  // Task name
    configMINIMAL_STACK_SIZE,  // Stack depth
    (void *)pdMS_TO_TICKS(40), // Completion time
    pdMS_TO_TICKS(200),        // Period
    pdMS_TO_TICKS(50),         // Relative Deadline
    NULL                       // Task handle
  );
}

// TEST 7: BARELY ADMISSIBLE BY PROCESSOR DEMAND (both U and demand are below upper bounds)
void edf_test_7() {
  xTaskCreatePeriodic(
    vPeriodicTask,             // Task function
    "Adm_A",                   // Task name
    configMINIMAL_STACK_SIZE,  // Stack depth
    (void *)pdMS_TO_TICKS(10), // Completion time
    pdMS_TO_TICKS(50),         // Period
    pdMS_TO_TICKS(50),         // Relative Deadline
    NULL                       // Task handle
  );
  xTaskCreatePeriodic(
    vPeriodicTask,             // Task function
    "Adm_B",                   // Task name
    configMINIMAL_STACK_SIZE,  // Stack depth
    (void *)pdMS_TO_TICKS(40), // Completion time
    pdMS_TO_TICKS(200),        // Period
    pdMS_TO_TICKS(50),         // Relative Deadline
    NULL                       // Task handle
  );
}

// TEST 6: BARELY NON-ADMISSIBLE BY UTILIZATION (10 tasks * 11ms = 110ms demand every 100ms)
// Total Utilization = 1.1 (110%)
void edf_test_6() {
  for (int i = 0; i < 10; i++) {
    char taskName[16];
    sprintf(taskName, "Fail_%d", i);
    xTaskCreatePeriodic(
      vPeriodicTask,             // Task function
      taskName,                  // Task name
      configMINIMAL_STACK_SIZE,  // Stack depth
      (void *)pdMS_TO_TICKS(11), // Completion time
      pdMS_TO_TICKS(100),        // Period
      pdMS_TO_TICKS(100),        // Relative Deadline
      NULL                       // Task handle
    );
  }
}

// TEST 5: BARELY ADMISSIBLE BY UTILIZATION (10 tasks * 10ms = 100ms demand every 100ms)
// Total Utilization = 1.0 (100%)
void edf_test_5() {
  for (int i = 0; i < 10; i++) {
    char taskName[16];
    sprintf(taskName, "Adm_%d", i);
    xTaskCreatePeriodic(
      vPeriodicTask,             // Task function
      taskName,                  // Task name
      configMINIMAL_STACK_SIZE,  // Stack depth
      (void *)pdMS_TO_TICKS(10), // Completion time
      pdMS_TO_TICKS(100),        // Period
      pdMS_TO_TICKS(100),        // Relative Deadline
      NULL                       // Task handle
    );
  }
}

// TEST4: 100 Tasks ADMISSIBLE
void edf_test_4() {
  for (int i = 0; i < 100; i++) {
    // NB: This breaks without downstream copying of task
    char taskName[16];
    sprintf(taskName, "test %d", i);
    xTaskCreatePeriodic(
      vPeriodicTask,            // Task function
      taskName,                 // Task name
      configMINIMAL_STACK_SIZE, // Stack depth
      (void *)pdMS_TO_TICKS(8), // Completion time
      pdMS_TO_TICKS(1000),      // Period
      pdMS_TO_TICKS(1000),      // Relative Deadline
      NULL                      // Task handle
    );
  }
}

// TEST3: 100 Tasks NON-ADMISSIBLE
void edf_test_3() {
  for (int i = 0; i < 100; i++) {
    char taskName[16];
    sprintf(taskName, "test %d", i);
    xTaskCreatePeriodic(
      vPeriodicTask,             // Task function
      taskName,                  // Task name
      configMINIMAL_STACK_SIZE,  // Stack depth
      (void *)pdMS_TO_TICKS(15), // Completion time
      pdMS_TO_TICKS(1000),       // Period
      pdMS_TO_TICKS(500),        // Relative Deadline
      NULL                       // Task handle
    );
  }
}

/**
 * Tests for Base Functionality
 */

// Test 2: Mark's Deadline DNE Period Smoke Test
void edf_test_2() {
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 1",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(200), // Completion time
    pdMS_TO_TICKS(600),         // Period
    pdMS_TO_TICKS(400),         // Relative Deadline
    NULL                        // Task handle
  );
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 2",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(200), // Completion time
    pdMS_TO_TICKS(800),         // Period
    pdMS_TO_TICKS(500),         // Relative Deadline
    NULL                        // Task handle
  );
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 3",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(300), // Completion time
    pdMS_TO_TICKS(900),         // Period
    pdMS_TO_TICKS(700),         // Relative Deadline
    NULL                        // Task handle
  );
}

// Smoke Test for Periodic Tasks (relative deadline == period)
void edf_test_1() {
  // Test 1: Periodic Task 1
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 1",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(200), // Completion time
    pdMS_TO_TICKS(600),         // Period
    pdMS_TO_TICKS(600),         // Relative Deadline
    NULL                        // Task handle
  );

  // Periodic Task 2
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 2",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(100), // Completion time
    pdMS_TO_TICKS(200),         // Period
    pdMS_TO_TICKS(200),         // Relative Deadline
    NULL                        // Task handle
  );
}
