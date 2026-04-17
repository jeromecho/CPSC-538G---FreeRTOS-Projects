#include "ProjectConfig.h"

#if TEST_SUITE == TEST_SUITE_PARTITIONED_MP

#include "partitioned_mp_tests.h"
#include "testing.h"

#include "edf_scheduler.h"
#include "smp_partitioned.h"

#include "helpers.h" // for crash_without_trace

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

  TaskHandle_t     runner0         = NULL;
  TaskHandle_t     runner1         = NULL;
  const BaseType_t runner0_created = xTaskCreate(
    vPartitionedMPTestRunner9Core0, "SMP9 Runner C0", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &runner0
  );
  const BaseType_t runner1_created = xTaskCreate(
    vPartitionedMPTestRunner9Core1, "SMP9 Runner C1", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &runner1
  );

  if (runner0_created != pdPASS || runner1_created != pdPASS) {
    vTaskSuspendAll();
    crash_without_trace("SMP9: Failed to create runner task(s)");
  }

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

  TaskHandle_t     runner0         = NULL;
  TaskHandle_t     runner1         = NULL;
  const BaseType_t runner0_created = xTaskCreate(
    vPartitionedMPTestRunner10Core0,
    "SMP10 Runner C0",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,
    &runner0
  );
  const BaseType_t runner1_created = xTaskCreate(
    vPartitionedMPTestRunner10Core1,
    "SMP10 Runner C1",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,
    &runner1
  );

  if (runner0_created != pdPASS || runner1_created != pdPASS) {
    vTaskSuspendAll();
    crash_without_trace("SMP10: Failed to create runner task(s); insufficient heap memory.");
  }

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

#if TEST_NR == 12
static TaskHandle_t g_smp12_remove_target_core0 = NULL;
static TaskHandle_t g_smp12_keep_target_core1   = NULL;

static void vPartitionedMPTestRunner12(void *pvParameters) {
  (void)pvParameters;

  vTaskDelay(pdMS_TO_TICKS(20));

  if (SMP_remove_task_from_core(g_smp12_remove_target_core0, 0) != pdPASS) {
    vTaskSuspendAll();
    crash_without_trace("SMP12: Failed to remove task from core 0");
  }

  if (EDF_get_task_by_handle(g_smp12_remove_target_core0) != NULL) {
    vTaskSuspendAll();
    crash_without_trace("SMP12: Removed task still present in scheduler");
  }

  if (EDF_get_task_by_handle(g_smp12_keep_target_core1) == NULL) {
    vTaskSuspendAll();
    crash_without_trace("SMP12: Unrelated core 1 task disappeared unexpectedly");
  }

  vTaskDelete(NULL);
}

void partitioned_mp_test_12() {
  const PeriodicTaskParams_t initial_tasks[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 6, 20, 20, 0},
    {EDF_periodic_task, 6, 20, 20, 1},
  };

  TMB_t *c0_task = NULL;
  TMB_t *c1_task = NULL;

  if (build_periodic_task_with_handle("SMP12 C0 Remove", &initial_tasks[0], &c0_task) != pdPASS ||
      build_periodic_task_with_handle("SMP12 C1 Keep", &initial_tasks[1], &c1_task) != pdPASS) {
    vTaskSuspendAll();
    crash_without_trace("SMP12: Failed to create setup tasks");
  }

  if (c0_task == NULL || c1_task == NULL) {
    vTaskSuspendAll();
    crash_without_trace("SMP12: Failed to resolve setup task handles");
  }

  g_smp12_remove_target_core0 = c0_task->handle;
  g_smp12_keep_target_core1   = c1_task->handle;

  TaskHandle_t runner = NULL;
  if (xTaskCreate(
        vPartitionedMPTestRunner12, "SMP12 Runner", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &runner
      ) != pdPASS) {
    vTaskSuspendAll();
    crash_without_trace("SMP12: Failed to create runner task");
  }
}
#endif // TEST_NR == 12

#if TEST_NR == 13
static TaskHandle_t g_smp13_migrate_source = NULL;

static void vPartitionedMPTestRunner13(void *pvParameters) {
  (void)pvParameters;

  vTaskDelay(pdMS_TO_TICKS(20));

  TMB_t *migrated = NULL;
  if (SMP_migrate_task_to_core(g_smp13_migrate_source, 1, &migrated) != pdPASS || migrated == NULL) {
    vTaskSuspendAll();
    crash_without_trace("SMP13: Migration API failed");
  }

  if (EDF_get_task_by_handle(g_smp13_migrate_source) != NULL) {
    vTaskSuspendAll();
    crash_without_trace("SMP13: Source handle still exists after migration");
  }

  if (migrated->assigned_core != 1) {
    vTaskSuspendAll();
    crash_without_trace("SMP13: Migrated task not assigned to destination core");
  }

  vTaskDelete(NULL);
}

void partitioned_mp_test_13() {
  const PeriodicTaskParams_t initial_tasks[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 5, 16, 16, 0},
    {EDF_periodic_task, 2, 16, 12, 1},
  };
  TMB_t *baseline = NULL;
  TMB_t *task     = NULL;
  if (build_periodic_task_with_handle("SMP13 Migrating", &initial_tasks[0], &task) != pdPASS ||
      build_periodic_task_with_handle("SMP13 Baseline", &initial_tasks[1], &baseline) != pdPASS || task == NULL ||
      baseline == NULL) {
    vTaskSuspendAll();
    crash_without_trace("SMP13: Failed to create initial task set");
  }

  g_smp13_migrate_source = task->handle;

  TaskHandle_t runner = NULL;
  if (xTaskCreate(
        vPartitionedMPTestRunner13, "SMP13 Runner", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &runner
      ) != pdPASS) {
    vTaskSuspendAll();
    crash_without_trace("SMP13: Failed to create runner task");
  }
}
#endif // TEST_NR == 13

#if TEST_NR == 14
/// @brief Migrate a single periodic task from core 0 to core 1.
/// Verifies that the migrated task executes on the correct core via trace output.
static TaskHandle_t g_smp14_migrate_target = NULL;

static void vPartitionedMPTestRunner14(void *pvParameters) {
  (void)pvParameters;

  vTaskDelay(pdMS_TO_TICKS(15));

  TMB_t *migrated = NULL;
  if (SMP_migrate_task_to_core(g_smp14_migrate_target, 1, &migrated) != pdPASS || migrated == NULL) {
    vTaskSuspendAll();
    crash_without_trace("SMP14: Migration failed");
  }

  vTaskDelete(NULL);
}

void partitioned_mp_test_14() {
  const PeriodicTaskParams_t initial_tasks[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 5, 12, 12, 0}, // Task to migrate
    {EDF_periodic_task, 3, 8,  8,  0}, // Baseline task on core 0
  };

  TMB_t *migrate_task = NULL;
  TMB_t *baseline     = NULL;

  if (build_periodic_task_with_handle("SMP14 Periodic C0, Migrate", &initial_tasks[0], &migrate_task) != pdPASS ||
      build_periodic_task_with_handle("SMP14 Periodic C0, Baseline", &initial_tasks[1], &baseline) != pdPASS ||
      migrate_task == NULL || baseline == NULL) {
    vTaskSuspendAll();
    crash_without_trace("SMP14: Failed to create initial tasks");
  }

  g_smp14_migrate_target = migrate_task->handle;

  TaskHandle_t runner = NULL;
  if (xTaskCreate(
        vPartitionedMPTestRunner14, "SMP14 Runner", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &runner
      ) != pdPASS) {
    vTaskSuspendAll();
    crash_without_trace("SMP14: Failed to create runner task");
  }
}
#endif // TEST_NR == 14

#endif // TEST_SUITE == TEST_SUITE_PARTITIONED_MP
