#include "edf_tests.h"


#if USE_EDF && !USE_SRP
// TODO: EDF test 5 needs updated logic for the admission (u=1)

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"

#include <stdio.h>

; // ===================================
; // === Local function declarations ===
; // ===================================

static void       build_periodic_task(const char *task_name, const EDF_PeriodicTaskParams_t *config);
static TickType_t build_periodic_test(
  const char *test_name, const EDF_PeriodicTaskParams_t *config, size_t num_tasks, TickType_t duration
);

; // ====================================
; // === Tests for Base Functionality ===
; // ====================================

// Smoke Test for Periodic Tasks (relative deadline == period)
TickType_t edf_test_1() {
  const EDF_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 6, 6},
    {EDF_periodic_task, 1, 2, 2},
  };
  return build_periodic_test( //
    "EDF Test 1",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    11
  );
}

// Test 2: Mark's Deadline DNE Period Smoke Test
TickType_t edf_test_2() {
  const EDF_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 6, 4},
    {EDF_periodic_task, 2, 8, 5},
    {EDF_periodic_task, 3, 9, 7},
  };
  return build_periodic_test( //
    "EDF Test 2",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    23
  );
}

; // ===============================
; // === Admission Control Tests ===
; // ===============================

// Note: in order to check the size of the created binary, run:
// arm-none-eabi-size build/Standard/main_blinky.elf
// TEST3: 100 Tasks NON-ADMISSIBLE
TickType_t edf_test_3() {
  EDF_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS];
  for (int i = 0; i < MAXIMUM_PERIODIC_TASKS; i++) {
    test_config[i].func = EDF_periodic_task;
    test_config[i].C    = 15;
    test_config[i].T    = 1000;
    test_config[i].D    = 500;
  }
  return build_periodic_test( //
    "EDF Test 3",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    1500
  );
}

// TEST4: 100 Tasks ADMISSIBLE
TickType_t edf_test_4() {
  EDF_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS];
  for (int i = 0; i < MAXIMUM_PERIODIC_TASKS; i++) {
    test_config[i].func = EDF_periodic_task;
    test_config[i].C    = 8;
    test_config[i].T    = 1000;
    test_config[i].D    = 1000;
  }
  return build_periodic_test( //
    "EDF Test 4",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    1500
  );
}

// TEST 5: BARELY ADMISSIBLE BY UTILIZATION (10 tasks * 10ms = 100ms demand every 100ms)
// Total Utilization = 1.0 (100%)
TickType_t edf_test_5() {
  EDF_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS];
  for (int i = 0; i < MAXIMUM_PERIODIC_TASKS; i++) {
    test_config[i].func = EDF_periodic_task;
    test_config[i].C    = 10;
    test_config[i].T    = 100;
    test_config[i].D    = 100;
  }
  return build_periodic_test( //
    "EDF Test 5",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    1500
  );
}

// TEST 6: BARELY NON-ADMISSIBLE BY UTILIZATION (10 tasks * 11ms = 110ms demand every 100ms)
// Total Utilization = 1.1 (110%)
TickType_t edf_test_6() {
  EDF_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS];
  for (int i = 0; i < MAXIMUM_PERIODIC_TASKS; i++) {
    test_config[i].func = EDF_periodic_task;
    test_config[i].C    = 11;
    test_config[i].T    = 100;
    test_config[i].D    = 100;
  }
  return build_periodic_test( //
    "EDF Test 6",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    1500
  );
}

// TEST 7: BARELY ADMISSIBLE BY PROCESSOR DEMAND (both U and demand are below upper bounds)
TickType_t edf_test_7() {
  const EDF_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 10, 50,  50},
    {EDF_periodic_task, 40, 200, 50},
  };
  return build_periodic_test( //
    "EDF Test 7",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    400
  );
}

// --- TEST 8: BARELY NON-ADMISSIBLE BY DEMAND (U is only 42% but demand > 1 at L = 50) ---
TickType_t edf_test_8() {
  const EDF_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 11, 50,  50},
    {EDF_periodic_task, 40, 200, 50},
  };
  return build_periodic_test( //
    "EDF Test 8",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    1500
  );
}

; // ==========================================================
; // === Tests for Drop-in of Tasks while System is Running ===
; // ==========================================================

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
  const EDF_PeriodicTaskParams_t task_config = {EDF_periodic_task, 400, 800, 800};
  build_periodic_task("EDF Test 9, Task 2", &task_config);

  vTaskDelete(NULL);
}
TickType_t edf_test_9() {
  const EDF_PeriodicTaskParams_t task_config = {EDF_periodic_task, 160, 800, 800};
  build_periodic_task("EDF Test 9, Task 1", &task_config);

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
  vTaskDelay(pdMS_TO_TICKS(500));

  // Drop-in Task: 90ms work, 200ms period (U=0.45)
  // At L=100, Demand = 20 + 90 = 110ms. PDC Violation!
  const EDF_PeriodicTaskParams_t task_config = {EDF_periodic_task, 90, 200, 100};
  build_periodic_task("EDF Test 10, Task 2", &task_config);

  vTaskDelete(NULL);
}
TickType_t edf_test_10() {
  const EDF_PeriodicTaskParams_t task_config = {EDF_periodic_task, 20, 100, 100};
  build_periodic_task("EDF Test 10, Task 1", &task_config);

  xTaskCreate( //
    vTestRunner10,
    "EDF Test 10, Test Runner",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,
    NULL
  );

  const TickType_t TEST_DURATION = 1000;
  return TEST_DURATION;
}

; // =================================
; // === Tests for Missed Deadline ===
; // =================================

// TEST 11: Missed Deadline (Total Utilization: 105%)
TickType_t edf_test_11() {
  const EDF_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 50,  120, 50 },
    {EDF_periodic_task, 130, 200, 200},
  };
  return build_periodic_test( //
    "EDF Test 11",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    250
  );
}


; // ==================================
; // === Local function definitions ===
; // ==================================

/// @brief Creates a periodic task from a provided task configuration
static void build_periodic_task(const char *task_name, const EDF_PeriodicTaskParams_t *config) {
  EDF_create_periodic_task( //
    config->func,
    task_name,
    pdMS_TO_TICKS(config->C),
    pdMS_TO_TICKS(config->T),
    pdMS_TO_TICKS(config->D),
    NULL
  );
}

/// @brief Creates all tasks from the provided test configuration for periodic tasks
static TickType_t build_periodic_test( //
  const char                     *test_name,
  const EDF_PeriodicTaskParams_t *config,
  size_t                          num_tasks,
  TickType_t                      duration
) {
  configASSERT(num_tasks == (MAXIMUM_PERIODIC_TASKS + MAXIMUM_APERIODIC_TASKS));

  for (size_t i = 0; i < num_tasks; i++) {
    char task_name[22]; // Exactly enough for "EDF Test XX, Task YYY", plus a null terminator byte
    snprintf(task_name, sizeof(task_name), "%s, Task %d", test_name, (int)(i + 1));
    build_periodic_task(task_name, &config[i]);
  }

  return duration;
}

#endif // USE_EDF && !USE_SRP