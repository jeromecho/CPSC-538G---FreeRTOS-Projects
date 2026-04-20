#include "edf_tests.h"

#if TEST_SUITE == TEST_SUITE_EDF

#include "edf_scheduler.h"
#include "testing.h"

; // ====================================
; // === Tests for Base Functionality ===
; // ====================================

#if TEST_NR == 1
// Smoke Test for Periodic Tasks (relative deadline == period)
void edf_test_1() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 6, 6},
    {EDF_periodic_task, 1, 2, 2},
  };
  build_periodic_test( //
    "EDF Test 1",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}

#elif TEST_NR == 2
// Test 2: Mark's Deadline DNE Period Smoke Test
void edf_test_2() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 6, 4},
    {EDF_periodic_task, 2, 8, 5},
    {EDF_periodic_task, 3, 9, 7},
  };
  build_periodic_test( //
    "EDF Test 2",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}

; // ===============================
; // === Admission Control Tests ===
; // ===============================

#elif TEST_NR == 3
// Note: in order to check the size of the created binary, run:
// arm-none-eabi-size build/Standard/main_blinky.elf
// TEST3: 100 Tasks NON-ADMISSIBLE
void edf_test_3() {
  PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS];
  for (int i = 0; i < MAXIMUM_PERIODIC_TASKS; i++) {
    test_config[i].func = EDF_periodic_task;
    test_config[i].C    = 15;
    test_config[i].T    = 1000;
    test_config[i].D    = 500;
  }
  build_periodic_test( //
    "EDF Test 3",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}

#elif TEST_NR == 4
// TEST4: 100 Tasks ADMISSIBLE
void edf_test_4() {
  PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS];
  for (int i = 0; i < MAXIMUM_PERIODIC_TASKS; i++) {
    test_config[i].func = EDF_periodic_task;
    test_config[i].C    = 8;
    test_config[i].T    = 1000;
    test_config[i].D    = 1000;
  }
  build_periodic_test( //
    "EDF Test 4",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}

#elif TEST_NR == 5
// TEST 5: BARELY ADMISSIBLE BY UTILIZATION (10 tasks * 10ms = 100ms demand every 100ms)
// Total Utilization = 1.0 (100%)
void edf_test_5() {
  PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS];
  for (int i = 0; i < MAXIMUM_PERIODIC_TASKS; i++) {
    test_config[i].func = EDF_periodic_task;
    test_config[i].C    = 10;
    test_config[i].T    = 100;
    test_config[i].D    = 100;
  }
  build_periodic_test( //
    "EDF Test 5",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}

#elif TEST_NR == 6
// TEST 6: BARELY NON-ADMISSIBLE BY UTILIZATION (10 tasks * 11ms = 110ms demand every 100ms)
// Total Utilization = 1.1 (110%)
void edf_test_6() {
  PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS];
  for (int i = 0; i < MAXIMUM_PERIODIC_TASKS; i++) {
    test_config[i].func = EDF_periodic_task;
    test_config[i].C    = 11;
    test_config[i].T    = 100;
    test_config[i].D    = 100;
  }
  build_periodic_test( //
    "EDF Test 6",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}

#elif TEST_NR == 7
// TEST 7: BARELY ADMISSIBLE BY PROCESSOR DEMAND (both U and demand are below upper bounds)
void edf_test_7() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 10, 50,  50},
    {EDF_periodic_task, 40, 200, 50},
  };
  build_periodic_test( //
    "EDF Test 7",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}

#elif TEST_NR == 8
// --- TEST 8: BARELY NON-ADMISSIBLE BY DEMAND (U is only 42% but demand > 1 at L = 50) ---
void edf_test_8() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 11, 50,  50},
    {EDF_periodic_task, 40, 200, 50},
  };
  build_periodic_test( //
    "EDF Test 8",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}

; // ==========================================================
; // === Tests for Drop-in of Tasks while System is Running ===
; // ==========================================================

#elif TEST_NR == 9
// TODO: Not sure if vTaskCreate calling xTaskCreatePeriodic, which calls vTaskCreate is a
//       good design. Should maybe create both tasks at the same time, and add a release time parameter so that the
//       scheduler can be responsible for running the tests

// TEST 9: Admissible Drop-in
void vTestRunner9() {
  // Wait 5 cycles (500ms) to show stable execution
  vTaskDelay(pdMS_TO_TICKS(500));

  // Drop-in Task: 400ms work, 800ms period.
  // Total Demand: 560ms < 800ms. Should pass PDC.
  // TODO: show why total demand is not exceeded
  const PeriodicTaskParams_t task_config = {EDF_periodic_task, 400, 800, 800};
  build_periodic_task("EDF Test 9, Task 2", &task_config);

  vTaskDelete(NULL);
}
void edf_test_9() {
  const PeriodicTaskParams_t task_config = {EDF_periodic_task, 160, 800, 800};
  build_periodic_task("EDF Test 9, Task 1", &task_config);

  TaskHandle_t test_runner_handle = NULL;
  xTaskCreate( //
    vTestRunner9,
    "EDF Test 9, Test Runner",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,
    &test_runner_handle
  );

#if (configUSE_CORE_AFFINITY == 1)
  if (test_runner_handle != NULL) {
    const UBaseType_t core_affinity_mask = ((UBaseType_t)1U) << configTICK_CORE;
    vTaskCoreAffinitySet(test_runner_handle, core_affinity_mask);
  }
#endif
}

#elif TEST_NR == 10
// TEST 10: Inadmissible Drop-in
void vTestRunner10() {
  // --- TEST B: Inadmissible Drop-in ---
  vTaskDelay(pdMS_TO_TICKS(500));

  // Drop-in Task: 90ms work, 200ms period (U=0.45)
  // At L=100, Demand = 20 + 90 = 110ms. PDC Violation!
  const PeriodicTaskParams_t task_config = {EDF_periodic_task, 90, 200, 100};
  build_periodic_task("EDF Test 10, Task 2", &task_config);

  vTaskDelete(NULL);
}
void edf_test_10() {
  const PeriodicTaskParams_t task_config = {EDF_periodic_task, 20, 100, 100};
  build_periodic_task("EDF Test 10, Task 1", &task_config);

  TaskHandle_t test_runner_handle = NULL;
  xTaskCreate( //
    vTestRunner10,
    "EDF Test 10, Test Runner",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,
    &test_runner_handle
  );

#if (configUSE_CORE_AFFINITY == 1)
  if (test_runner_handle != NULL) {
    const UBaseType_t core_affinity_mask = ((UBaseType_t)1U) << configTICK_CORE;
    vTaskCoreAffinitySet(test_runner_handle, core_affinity_mask);
  }
#endif
}

; // =================================
; // === Tests for Missed Deadline ===
; // =================================

#elif TEST_NR == 11
// TEST 11: Missed Deadline (Total Utilization: 105%)
void edf_test_11() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 50,  120, 50 },
    {EDF_periodic_task, 130, 200, 200},
  };
  build_periodic_test( //
    "EDF Test 11",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}
#endif

#endif // TEST_SUITE == TEST_SUITE_EDF
