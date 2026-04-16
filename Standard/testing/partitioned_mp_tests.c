#include "ProjectConfig.h"

#if TEST_SUITE == TEST_SUITE_PARTITIONED_MP

#include "partitioned_mp_tests.h"
#include "testing.h"

#include "edf_scheduler.h"

#include <stdio.h>

static void build_periodic_tasks_on_core(
  const char *test_name, const PeriodicTaskParams_t *base_config, size_t num_tasks, uint8_t core
) {
  for (size_t i = 0; i < num_tasks; ++i) {
    PeriodicTaskParams_t config = base_config[i];
    config.core                 = core;

    char task_name[32];
    snprintf(task_name, sizeof(task_name), "%s C%d, T%02d", test_name, (int)core, (int)(i + 1));
    build_periodic_task(task_name, &config);
  }
}

static void
build_periodic_tasks_on_all_cores(const char *test_name, const PeriodicTaskParams_t *base_config, size_t num_tasks) {
  for (uint8_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    build_periodic_tasks_on_core(test_name, base_config, num_tasks, core);
  }
}

#if TEST_NR == 1
/// @brief Dual-core EDF smoke test with one core running two tasks and the other running one task.
/// The shorter-period task on core 0 should preempt the longer-period task on the same core.
void partitioned_mp_test_1() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 6, 6, 0},
    {EDF_periodic_task, 1, 2, 2, 0},
  };

  build_periodic_tasks_on_all_cores("SMP Test 1", test_config, MAXIMUM_PERIODIC_TASKS);
}
#endif // TEST_NR == 1

#if TEST_NR == 2
void partitioned_mp_test_2() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 6, 4, 0},
    {EDF_periodic_task, 2, 8, 5, 0},
    {EDF_periodic_task, 3, 9, 7, 0},
  };
  build_periodic_tasks_on_all_cores("SMP Test 2", test_config, MAXIMUM_PERIODIC_TASKS);
}
#endif // TEST_NR == 2

#if TEST_NR == 3
void partitioned_mp_test_3() {
  for (uint8_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    for (size_t i = 0; i < MAXIMUM_PERIODIC_TASKS; ++i) {
      PeriodicTaskParams_t config = {EDF_periodic_task, 15, 1000, 500, core};
      char                 name[32];
      snprintf(name, sizeof(name), "SMP T3 C%d, T%02d", (int)core, (int)(i + 1));
      build_periodic_task(name, &config);
    }
  }
}
#endif // TEST_NR == 3

#if TEST_NR == 4
void partitioned_mp_test_4() {
  for (uint8_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    for (size_t i = 0; i < MAXIMUM_PERIODIC_TASKS; ++i) {
      PeriodicTaskParams_t config = {EDF_periodic_task, 8, 1000, 1000, core};
      char                 name[32];
      snprintf(name, sizeof(name), "SMP T4 C%d, T%02d", (int)core, (int)(i + 1));
      build_periodic_task(name, &config);
    }
  }
}
#endif // TEST_NR == 4

#if TEST_NR == 5
void partitioned_mp_test_5() {
  for (uint8_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    for (size_t i = 0; i < MAXIMUM_PERIODIC_TASKS; ++i) {
      PeriodicTaskParams_t config = {EDF_periodic_task, 10, 100, 100, core};
      char                 name[32];
      snprintf(name, sizeof(name), "SMP T5 C%d, T%02d", (int)core, (int)(i + 1));
      build_periodic_task(name, &config);
    }
  }
}
#endif // TEST_NR == 5

#if TEST_NR == 6
void partitioned_mp_test_6() {
  for (uint8_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    for (size_t i = 0; i < MAXIMUM_PERIODIC_TASKS; ++i) {
      PeriodicTaskParams_t config = {EDF_periodic_task, 11, 100, 100, core};
      char                 name[32];
      snprintf(name, sizeof(name), "SMP T6 C%d, T%02d", (int)core, (int)(i + 1));
      build_periodic_task(name, &config);
    }
  }
}
#endif // TEST_NR == 6

#if TEST_NR == 7
void partitioned_mp_test_7() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 10, 50,  50, 0},
    {EDF_periodic_task, 40, 200, 50, 0},
  };
  build_periodic_tasks_on_all_cores("SMP Test 7", test_config, MAXIMUM_PERIODIC_TASKS);
}
#endif // TEST_NR == 7

#if TEST_NR == 8
void partitioned_mp_test_8() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 11, 50,  50, 0},
    {EDF_periodic_task, 40, 200, 50, 0},
  };
  build_periodic_tasks_on_all_cores("SMP Test 8", test_config, MAXIMUM_PERIODIC_TASKS);
}
#endif // TEST_NR == 8

#if TEST_NR == 9
static void vPartitionedMPTestRunner9Core0(void *pvParameters) {
  (void)pvParameters;
  vTaskDelay(pdMS_TO_TICKS(500));
  const PeriodicTaskParams_t config = {EDF_periodic_task, 400, 800, 800, 0};
  build_periodic_task("SMP Test 9 C0, Drop-in", &config);
  vTaskDelete(NULL);
}

static void vPartitionedMPTestRunner9Core1(void *pvParameters) {
  (void)pvParameters;
  vTaskDelay(pdMS_TO_TICKS(500));
  const PeriodicTaskParams_t config = {EDF_periodic_task, 400, 800, 800, 1};
  build_periodic_task("SMP Test 9 C1, Drop-in", &config);
  vTaskDelete(NULL);
}

void partitioned_mp_test_9() {
  const PeriodicTaskParams_t base0 = {EDF_periodic_task, 160, 800, 800, 0};
  const PeriodicTaskParams_t base1 = {EDF_periodic_task, 160, 800, 800, 1};
  build_periodic_task("SMP Test 9 C0, Base", &base0);
  build_periodic_task("SMP Test 9 C1, Base", &base1);

  TaskHandle_t runner0 = NULL;
  TaskHandle_t runner1 = NULL;
  xTaskCreate(
    vPartitionedMPTestRunner9Core0, "SMP9 Runner C0", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &runner0
  );
  xTaskCreate(
    vPartitionedMPTestRunner9Core1, "SMP9 Runner C1", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &runner1
  );

#if (configUSE_CORE_AFFINITY == 1)
  if (runner0 != NULL) {
    const UBaseType_t mask0 = ((UBaseType_t)1U) << 0;
    vTaskCoreAffinitySet(runner0, mask0);
  }
  if (runner1 != NULL) {
    const UBaseType_t mask1 = ((UBaseType_t)1U) << 1;
    vTaskCoreAffinitySet(runner1, mask1);
  }
#endif
}
#endif // TEST_NR == 9

#if TEST_NR == 10
static void vPartitionedMPTestRunner10Core0(void *pvParameters) {
  (void)pvParameters;
  vTaskDelay(pdMS_TO_TICKS(500));
  const PeriodicTaskParams_t config = {EDF_periodic_task, 90, 200, 100, 0};
  build_periodic_task("SMP Test 10 C0, Drop-in", &config);
  vTaskDelete(NULL);
}

static void vPartitionedMPTestRunner10Core1(void *pvParameters) {
  (void)pvParameters;
  vTaskDelay(pdMS_TO_TICKS(500));
  const PeriodicTaskParams_t config = {EDF_periodic_task, 90, 200, 100, 1};
  build_periodic_task("SMP Test 10 C1, Drop-in", &config);
  vTaskDelete(NULL);
}

void partitioned_mp_test_10() {
  const PeriodicTaskParams_t base0 = {EDF_periodic_task, 20, 100, 100, 0};
  const PeriodicTaskParams_t base1 = {EDF_periodic_task, 20, 100, 100, 1};
  build_periodic_task("SMP Test 10 C0, Base", &base0);
  build_periodic_task("SMP Test 10 C1, Base", &base1);

  TaskHandle_t runner0 = NULL;
  TaskHandle_t runner1 = NULL;
  xTaskCreate(
    vPartitionedMPTestRunner10Core0,
    "SMP10 Runner C0",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,
    &runner0
  );
  xTaskCreate(
    vPartitionedMPTestRunner10Core1,
    "SMP10 Runner C1",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,
    &runner1
  );

#if (configUSE_CORE_AFFINITY == 1)
  if (runner0 != NULL) {
    const UBaseType_t mask0 = ((UBaseType_t)1U) << 0;
    vTaskCoreAffinitySet(runner0, mask0);
  }
  if (runner1 != NULL) {
    const UBaseType_t mask1 = ((UBaseType_t)1U) << 1;
    vTaskCoreAffinitySet(runner1, mask1);
  }
#endif
}
#endif // TEST_NR == 10

#if TEST_NR == 11
void partitioned_mp_test_11() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 50,  120, 50,  0},
    {EDF_periodic_task, 130, 200, 200, 0},
  };
  build_periodic_tasks_on_all_cores("SMP Test 11", test_config, MAXIMUM_PERIODIC_TASKS);
}
#endif // TEST_NR == 11

#endif // TEST_SUITE == TEST_SUITE_PARTITIONED_MP
