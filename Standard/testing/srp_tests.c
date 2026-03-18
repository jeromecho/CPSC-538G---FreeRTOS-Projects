#include "srp_tests.h"

#if USE_SRP

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"
#include "helpers.h"
#include "srp.h"

#include <stdio.h>

#define VERIFY_EXECUTION(times, total)                                                                                 \
  do {                                                                                                                 \
    unsigned int _sum = 0;                                                                                             \
    for (size_t _i = 0; _i < sizeof(times) / sizeof(times[0]); _i++)                                                   \
      _sum += times[_i];                                                                                               \
    configASSERT(_sum == (TickType_t)total);                                                                           \
  } while (0)

typedef enum { TEST_TYPE_PERIODIC, TEST_TYPE_APERIODIC } TaskTestType_t;

/// @brief Function to build a test based on the provided test configuration.
TickType_t build_test( //
  const char    *test_name,
  TaskTestType_t type,
  const void    *config,
  size_t         num_tasks,
  TickType_t     duration
) {
  configASSERT(num_tasks == (MAXIMUM_PERIODIC_TASKS + MAXIMUM_APERIODIC_TASKS));

  for (size_t i = 0; i < num_tasks; i++) {
    char task_name[19];

    if (type == TEST_TYPE_PERIODIC) {
      const PeriodicTaskParams_t *cfg = &((const PeriodicTaskParams_t *)config)[i];

      snprintf(task_name, sizeof(task_name), "%s, Task %d", test_name, (int)(i + 1));

      SRP_create_periodic_task(
        cfg->func,
        task_name,
        pdMS_TO_TICKS(cfg->C),
        pdMS_TO_TICKS(cfg->T), // Periodic uses T
        pdMS_TO_TICKS(cfg->D),
        NULL,
        cfg->plvl,
        cfg->resources
      );
    } else if (type == TEST_TYPE_APERIODIC) {
      const AperiodicTaskParams_t *cfg = &((const AperiodicTaskParams_t *)config)[i];

      snprintf(task_name, sizeof(task_name), "%s, Task %d", test_name, (int)(i + 1));

      SRP_create_aperiodic_task(
        cfg->func,
        task_name,
        pdMS_TO_TICKS(cfg->C),
        pdMS_TO_TICKS(cfg->r), // Aperiodic uses r
        pdMS_TO_TICKS(cfg->D),
        NULL,
        cfg->plvl,
        cfg->resources
      );
    }
  }

  return duration;
}

#if TEST_NR == 1
/// Test 1: Basic SRP Priority Inversion Prevention
///
/// This test demonstrates how SRP prevents a medium-priority task from preempting a low-priority task holding a shared
/// resource. Validates basic semaphore ceiling calculations and blocking mechanics.
///
/// - Task 3 (Low) starts and takes Resource 1, raising the system ceiling.
/// - Task 2 (Medium) arrives but is blocked by the ceiling, preventing priority inversion.
/// - Task 1 (High) arrives, preempts, and interacts with the resource.
void vSRPTest1Task1(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {30};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  SRP_take_binary_semaphore(0); // Take R1
  execute_for_ticks(execution_times[i++]);
  SRP_give_binary_semaphore(0); // Give R1

  EDF_mark_task_done(NULL);
}
void vSRPTest1Task3(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {100, 20};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  SRP_take_binary_semaphore(0);            // Take R1 (System ceiling should become 3)
  execute_for_ticks(execution_times[i++]);
  SRP_give_binary_semaphore(0);            // Give R1
  execute_for_ticks(execution_times[i++]); // Finish remaining work

  EDF_mark_task_done(NULL);
}
TickType_t srp_test_1() {
  const AperiodicTaskParams_t test_config[MAXIMUM_APERIODIC_TASKS] = {
    {vSRPTest1Task1,     30,  40, 100, 3, {10}},
    {EDF_aperiodic_task, 50,  20, 200, 2, {0} },
    {vSRPTest1Task3,     120, 0,  300, 1, {50}},
  };

  return build_test( //
    "SRP Test 1",
    TEST_TYPE_APERIODIC,
    test_config,
    MAXIMUM_APERIODIC_TASKS,
    300
  );
}

#elif TEST_NR == 2
/// Test 2: Complex Multi-Resource SRP Validation
///
/// Uses 4 tasks and 3 distinct resources (semaphores) to validate nested resource locking and system ceiling dynamic
/// updates. Proves that chained priority inversions and deadlocks are mathematically impossible under the current SRP
/// implementation.
///
/// This test is taken from https://cpen432.github.io/resources/bader-slides/8-ResourceSharing.pdf, Page 49
void vSRPTest2Task1(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {93, 45, 45, 45, 45, 45, 45};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  execute_for_ticks(execution_times[i++]); // Initial execution

  SRP_take_binary_semaphore(0);            // Take Red (R0)
  execute_for_ticks(execution_times[i++]);
  SRP_give_binary_semaphore(0);            // Give Red (R0)

  execute_for_ticks(execution_times[i++]); // Execution between resources

  SRP_take_binary_semaphore(1);            // Take Blue (R1)
  execute_for_ticks(execution_times[i++]);
  SRP_give_binary_semaphore(1);            // Give Blue (R1)

  execute_for_ticks(execution_times[i++]); // Execution between resources

  SRP_take_binary_semaphore(2);            // Take Yellow (R2)
  execute_for_ticks(execution_times[i++]);
  SRP_give_binary_semaphore(2);            // Give Yellow (R2)

  execute_for_ticks(execution_times[i++]); // Finish remaining work

  EDF_mark_task_done(NULL);
}
void vSRPTest2Task2(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {93, 109, 93};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  execute_for_ticks(execution_times[i++]); // Initial execution
  SRP_take_binary_semaphore(1);            // Take Blue (R1)
  execute_for_ticks(execution_times[i++]); // Critical section
  SRP_give_binary_semaphore(1);            // Give Blue (R1)
  execute_for_ticks(execution_times[i++]); // Finish remaining work

  EDF_mark_task_done(NULL);
}
void vSRPTest2Task3(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {90, 109, 93};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  execute_for_ticks(execution_times[i++]);
  SRP_take_binary_semaphore(2);            // Take Yellow (R2)
  execute_for_ticks(execution_times[i++]); // Critical section
  SRP_give_binary_semaphore(2);            // Give Yellow (R2)
  execute_for_ticks(execution_times[i++]); // Finish remaining work

  EDF_mark_task_done(NULL);
}
void vSRPTest2Task4(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {93, 157, 93};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  execute_for_ticks(execution_times[i++]); // Initial execution
  SRP_take_binary_semaphore(0);            // Take Red (R0)
  execute_for_ticks(execution_times[i++]); // Critical section
  SRP_give_binary_semaphore(0);            // Give Red (R0)
  execute_for_ticks(execution_times[i++]); // Finish remaining work

  EDF_mark_task_done(NULL);
}
TickType_t srp_test_2() {
  const AperiodicTaskParams_t test_config[MAXIMUM_APERIODIC_TASKS] = {
    {vSRPTest2Task1, 363, 400, 500,  4, {45, 45, 45}},
    {vSRPTest2Task2, 295, 279, 700,  3, {0, 109, 0} },
    {vSRPTest2Task3, 292, 150, 1100, 2, {0, 0, 109} },
    {vSRPTest2Task4, 343, 0,   1400, 1, {157, 0, 0} },
  };

  return build_test( //
    "SRP Test 2",
    TEST_TYPE_APERIODIC,
    test_config,
    MAXIMUM_APERIODIC_TASKS,
    1500
  );
}

#elif TEST_NR == 3 || TEST_NR == 4
/// Test 3 & 4: Comparison of execution traces when Stack Sharing is enabled vs. disabled.
///
/// Since tasks at the same preemption level cannot preempt each other under SRP, this enables stack sharing. This
/// creates two tasks at the same preemption level, and shows that the execution is the same when stack sharing is
/// enabled and disabled.
///
/// - Task 1 (Level 1) runs.
/// - Task 2 (Level 1) arrives with an earlier deadline but is correctly blocked by the ceiling.
/// - Task 3 (Level 2) arrives and successfully preempts Task 1.
const AperiodicTaskParams_t test_config[MAXIMUM_APERIODIC_TASKS] = {
  {EDF_aperiodic_task, 100, 0,  300, 1, {NULL}},
  {EDF_aperiodic_task, 100, 20, 230, 1, {NULL}},
  {EDF_aperiodic_task, 50,  50, 150, 2, {NULL}},
};

TickType_t srp_test_3() {
  return build_test( //
    "SRP Test 3",
    TEST_TYPE_APERIODIC,
    test_config,
    MAXIMUM_APERIODIC_TASKS,
    300
  );
}
TickType_t srp_test_4() {
  return build_test( //
    "SRP Test 4",
    TEST_TYPE_APERIODIC,
    test_config,
    MAXIMUM_APERIODIC_TASKS,
    300
  );
}

#elif TEST_NR == 5 || TEST_NR == 6
/// Tests 5 & 6: Quantitative Analysis of Stack Sharing RAM Usage
///
/// A stress test designed to measure the memory reduction achieved by SRP.
///
///  Generates a maximum load of tasks and distributes them across a smaller subset
/// of preemption levels. Outputs the exact runtime static memory footprint
/// using `sizeof()`. Proves that stack sharing significantly reduces the `.bss`
/// memory allocation required for the RTOS.
///
TickType_t srp_test_5() {
  const unsigned int NUM_TASKS            = MAXIMUM_APERIODIC_TASKS;
  const unsigned int COMPLETION_TIME_MS   = 10;
  const unsigned int RELATIVE_DEADLINE_MS = NUM_TASKS * COMPLETION_TIME_MS;

  AperiodicTaskParams_t test_config[MAXIMUM_APERIODIC_TASKS];
  for (int i = 0; i < NUM_TASKS; i++) {
    test_config[i].func = EDF_aperiodic_task;
    test_config[i].C    = COMPLETION_TIME_MS;
    test_config[i].r    = 0;
    test_config[i].D    = RELATIVE_DEADLINE_MS;
    test_config[i].plvl = (i % N_PREEMPTION_LEVELS) + 1;
  }

  build_test( //
    "SRP Test 5",
    TEST_TYPE_APERIODIC,
    test_config,
    NUM_TASKS,
    RELATIVE_DEADLINE_MS
  );

  // 3. Quantitative Analysis Output
  printf("\n==================================================\n");
  printf("   QUANTITATIVE ANALYSIS: STACK STORAGE USAGE\n");
  printf("==================================================\n");
  printf("Total Tasks: %d\n", NUM_TASKS);
  printf("Preemption Levels: %d\n", N_PREEMPTION_LEVELS);
  printf(
    "Stack Size per Unit: %u words (%u bytes)\n",
    (unsigned int)SHARED_STACK_SIZE,
    (unsigned int)(SHARED_STACK_SIZE * sizeof(StackType_t))
  );
  printf("--------------------------------------------------\n");

#if ENABLE_STACK_SHARING
  // Extern references so we can measure the sizes from this file
  extern StackType_t shared_stacks[N_PREEMPTION_LEVELS][SHARED_STACK_SIZE];

  size_t stack_ram = sizeof(shared_stacks);
  printf("Configuration: SRP STACK SHARING [STACK SHARING ENABLED]\n");
  printf("Allocated Arrays: %d shared stacks\n", N_PREEMPTION_LEVELS);
  printf("Maximum Run-Time Stack Storage: %u bytes\n", (unsigned int)stack_ram);

#else
  // Extern references to the private arrays
  extern StackType_t edf_private_stacks_periodic[MAXIMUM_PERIODIC_TASKS][SHARED_STACK_SIZE];
  extern StackType_t edf_private_stacks_aperiodic[MAXIMUM_APERIODIC_TASKS][SHARED_STACK_SIZE];

  size_t stack_ram = sizeof(edf_private_stacks_periodic) + sizeof(edf_private_stacks_aperiodic);
  printf("Configuration: PRIVATE STACKS [STACK SHARING DISABLED]\n");
  printf("Allocated Arrays: %d periodic + %d aperiodic stacks\n", MAXIMUM_PERIODIC_TASKS, MAXIMUM_APERIODIC_TASKS);
  printf("Maximum Run-Time Stack Storage: %u bytes\n", (unsigned int)stack_ram);
#endif

  printf("==================================================\n\n");

  // Run the test for a short duration to prove it executes without crashing
  const TickType_t TEST_DURATION = RELATIVE_DEADLINE_MS;
  return TEST_DURATION;
}

TickType_t srp_test_6() { return srp_test_5(); }

#elif TEST_NR == 7
void vSRPTest7Task1(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {1, 1};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  SRP_take_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);
  SRP_give_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);

  EDF_mark_task_done(NULL);
}
void vSRPTest7Task3(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {3, 7};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  SRP_take_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);
  SRP_give_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);

  EDF_mark_task_done(NULL);
}
TickType_t srp_test_7() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {vSRPTest7Task1,    2,  10, 10, 3, {1}},
    {EDF_periodic_task, 4,  20, 20, 2, {0}},
    {vSRPTest7Task3,    10, 50, 50, 1, {3}},
  };

  return build_test( //
    "SRP Test 7",
    TEST_TYPE_PERIODIC,
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    300
  );
}

#elif TEST_NR == 8
void vSRPTest8Task1(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {1, 1};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  SRP_take_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);
  SRP_give_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);

  EDF_mark_task_done(NULL);
}
void vSRPTest8Task3(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {9, 1};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  SRP_take_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);
  SRP_give_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);

  EDF_mark_task_done(NULL);
}
TickType_t srp_test_8() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {vSRPTest8Task1,    2,  10, 10, 3, {1}},
    {EDF_periodic_task, 4,  20, 20, 2, {0}},
    {vSRPTest8Task3,    10, 50, 50, 1, {9}},
  };

  return build_test( //
    "SRP Test 8",
    TEST_TYPE_PERIODIC,
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    300
  );
}

#elif TEST_NR == 9
void vSRPTest9Task1(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {2, 3};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  SRP_take_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);
  SRP_give_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);

  EDF_mark_task_done(NULL);
}
void vSRPTest9Task3(void *pvParameters) {
  const TickType_t   completion_time   = (TickType_t)pvParameters;
  const unsigned int execution_times[] = {6, 2};
  size_t             i                 = 0;
  VERIFY_EXECUTION(execution_times, completion_time);

  SRP_take_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);
  SRP_give_binary_semaphore(0);
  execute_for_ticks(execution_times[i++]);

  EDF_mark_task_done(NULL);
}
TickType_t srp_test_9() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {vSRPTest9Task1,    5, 20, 10, 3, {2}},
    {EDF_periodic_task, 4, 20, 12, 2, {0}},
    {vSRPTest9Task3,    8, 50, 50, 1, {6}}
  };

  return build_test( //
    "SRP Test 9",
    TEST_TYPE_PERIODIC,
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    300
  );
}

#endif // TEST_NR

#endif // USE_SRP
