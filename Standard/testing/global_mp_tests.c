#include "ProjectConfig.h"

#if TEST_SUITE == TEST_SUITE_GLOBAL_MP

#include "global_mp_tests.h"
#include "testing.h"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"

// Consult SMP Documentation for more information on tests
#if TEST_NR == 1
void global_mp_test_1() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 4, 4}
  };
  build_periodic_test("SMP (Global) Test 1", test_config, MAXIMUM_PERIODIC_TASKS);
}

#elif TEST_NR == 2
void global_mp_test_2() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 4, 4},
    {EDF_periodic_task, 3, 4, 4}
  };
  build_periodic_test("SMP (Global) Test 2", test_config, MAXIMUM_PERIODIC_TASKS);
}

#elif TEST_NR == 3
void global_mp_test_3() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2,  4,  4 },
    {EDF_periodic_task, 10, 20, 20},
    {EDF_periodic_task, 10, 20, 20}
  };
  build_periodic_test("SMP (Global) Test 3", test_config, MAXIMUM_PERIODIC_TASKS);
}

#elif TEST_NR == 4
void global_mp_test_4() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 4,  4 },
    {EDF_periodic_task, 1, 5,  5 },
    {EDF_periodic_task, 5, 10, 10}
  };
  build_periodic_test("SMP (Global) Test 4", test_config, MAXIMUM_PERIODIC_TASKS);
}

#elif TEST_NR == 5
void global_mp_test_5() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 4, 4},
    {EDF_periodic_task, 3, 7, 7},
    {EDF_periodic_task, 3, 5, 5}
  };
  build_periodic_test("SMP (Global) Test 5", test_config, MAXIMUM_PERIODIC_TASKS);
}

#elif TEST_NR == 6
void global_mp_test_6() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 8, 10, 10},
    {EDF_periodic_task, 8, 10, 10},
    {EDF_periodic_task, 6, 10, 10}
  };
  build_periodic_test("SMP (Global) Test 6", test_config, MAXIMUM_PERIODIC_TASKS);
}

#elif TEST_NR == 7
void global_mp_test_7() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 6, 10, 10},
    {EDF_periodic_task, 6, 10, 10},
    {EDF_periodic_task, 3, 4,  4 }
  };
  build_periodic_test("SMP (Global) Test 7", test_config, MAXIMUM_PERIODIC_TASKS);
}

#elif TEST_NR == 8
void global_mp_test_8() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 5, 10, 10},
    {EDF_periodic_task, 5, 10, 10},
    {EDF_periodic_task, 4, 5,  5 }
  };
  build_periodic_test("SMP (Global) Test 8", test_config, MAXIMUM_PERIODIC_TASKS);
}

#elif TEST_NR == 9
void global_mp_test_9() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 6, 10, 8},
    {EDF_periodic_task, 8, 10, 9},
    {EDF_periodic_task, 7, 10, 8}
  };
  build_periodic_test("SMP (Global) Test 9", test_config, MAXIMUM_PERIODIC_TASKS);
}

#elif TEST_NR == 10
void global_mp_test_10() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 6, 10, 8},
    {EDF_periodic_task, 6, 10, 8},
    {EDF_periodic_task, 3, 10, 3}
  };
  build_periodic_test("SMP (Global) Test 10", test_config, MAXIMUM_PERIODIC_TASKS);
}

#elif TEST_NR == 11
void global_mp_test_11() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 3, 8,  6},
    {EDF_periodic_task, 3, 8,  6},
    {EDF_periodic_task, 2, 12, 3}
  };
  build_periodic_test("SMP (Global) Test 11", test_config, MAXIMUM_PERIODIC_TASKS);
}

#elif TEST_NR == 12
void global_mp_test_12() {
  for (size_t i = 0; i < 100; ++i) {
    PeriodicTaskParams_t config = {EDF_periodic_task, 7, 1000, 1000};
    char                 name[32];
    snprintf(name, sizeof(name), "SMP T4 T%02d", (int)(i + 1));
    build_periodic_task(name, &config);
  }
}

#elif TEST_NR == 13
void global_mp_test_13() {
  for (size_t i = 0; i < 100; ++i) {
    PeriodicTaskParams_t config = {EDF_periodic_task, 25, 1000, 1000};
    char                 name[32];
    snprintf(name, sizeof(name), "SMP T4 T%02d", (int)(i + 1));
    build_periodic_task(name, &config);
  }
}

#elif TEST_NR == 14
static void vTestRunner14(void *pvParameters) {
  (void)pvParameters;
  vTaskDelay(pdMS_TO_TICKS(20));
  const PeriodicTaskParams_t config = {EDF_periodic_task, 4, 8, 8};
  build_periodic_task("SMP (Global) Test 14 Drop-in", &config);
  vTaskDelete(NULL);
}

void global_mp_test_14() {
  const PeriodicTaskParams_t base0 = {EDF_periodic_task, 4, 8, 8};
  const PeriodicTaskParams_t base1 = {EDF_periodic_task, 4, 8, 8};
  build_periodic_task("SMP (Global) Test 14 C0", &base0);
  build_periodic_task("SMP (Global) Test 14 C1", &base1);

  xTaskCreate( //
    vTestRunner14,
    "GLOBAL MP TEST 14 RUNNER",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,
    NULL
  );
}

#elif TEST_NR == 15
static void vTestRunner15(void *pvParameters) {
  (void)pvParameters;
  vTaskDelay(pdMS_TO_TICKS(20));
  const PeriodicTaskParams_t config = {EDF_periodic_task, 5, 8, 8};
  build_periodic_task("SMP (Global) Test 15 Drop-in", &config);
  vTaskDelete(NULL);
}

void global_mp_test_15() {
  const PeriodicTaskParams_t base0 = {EDF_periodic_task, 4, 8, 8};
  const PeriodicTaskParams_t base1 = {EDF_periodic_task, 4, 8, 8};
  build_periodic_task("SMP (Global) Test 15 C0", &base0);
  build_periodic_task("SMP (Global) Test 15 C1", &base1);

  xTaskCreate( //
    vTestRunner15,
    "GLOBAL MP TEST 15 RUNNER",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,
    NULL
  );
}

#endif // TEST_NR

#endif // TEST_SUITE == TEST_SUITE_MP
