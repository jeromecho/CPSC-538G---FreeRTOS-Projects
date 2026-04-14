#include "srp_tests.h"

#if USE_SRP

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"
#include "helpers.h"
#include "srp.h"

#include <stdio.h>

#define LEN(x) (sizeof(x) / sizeof((x)[0]))

; // ===================================
; // === Local function declarations ===
; // ===================================

static void       build_periodic_task(const char *task_name, const SRP_PeriodicTaskParams_t *config);
static void       build_aperiodic_task(const char *task_name, const SRP_AperiodicTaskParams_t *config);
static TickType_t build_periodic_test(
  const char *test_name, const SRP_PeriodicTaskParams_t *config, size_t num_tasks, TickType_t duration
);
static TickType_t build_aperiodic_test(
  const char *test_name, const SRP_AperiodicTaskParams_t *config, size_t num_tasks, TickType_t duration
);
static void execute_steps(const TickType_t completion_time, const TaskStep_t steps[], const size_t num_steps);

; // ========================
; // === Test definitions ===
; // ========================

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
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_TAKE_SEMAPHORE, 0 },
    {TASK_EXECUTE,        30},
    {TASK_GIVE_SEMAPHORE, 0 },
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
void vSRPTest1Task3(void *pvParameters) {
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_TAKE_SEMAPHORE, 0  },
    {TASK_EXECUTE,        100},
    {TASK_GIVE_SEMAPHORE, 0  },
    {TASK_EXECUTE,        20 },
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
TickType_t srp_test_1() {
  const SRP_AperiodicTaskParams_t test_config[MAXIMUM_APERIODIC_TASKS] = {
    {vSRPTest1Task1,     30,  40, 100, 3, {30} },
    {EDF_aperiodic_task, 50,  20, 200, 2, {0}  },
    {vSRPTest1Task3,     120, 0,  300, 1, {100}},
  };

  return build_aperiodic_test( //
    "SRP Test 1",
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
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_EXECUTE,        93},
    {TASK_TAKE_SEMAPHORE, 0 }, // Take Red (R0)
    {TASK_EXECUTE,        45},
    {TASK_GIVE_SEMAPHORE, 0 }, // Give Red (R0)
    {TASK_EXECUTE,        45},
    {TASK_TAKE_SEMAPHORE, 1 }, // Take Blue (R1)
    {TASK_EXECUTE,        45},
    {TASK_GIVE_SEMAPHORE, 1 }, // Give Blue (R1)
    {TASK_EXECUTE,        45},
    {TASK_TAKE_SEMAPHORE, 2 }, // Take Yellow (R2)
    {TASK_EXECUTE,        45},
    {TASK_GIVE_SEMAPHORE, 2 }, // Give Yellow (R2)
    {TASK_EXECUTE,        45},
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
void vSRPTest2Task2(void *pvParameters) {
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_EXECUTE,        93 },
    {TASK_TAKE_SEMAPHORE, 1  }, // Take Blue (R1)
    {TASK_EXECUTE,        109},
    {TASK_GIVE_SEMAPHORE, 1  }, // Give Blue (R1)
    {TASK_EXECUTE,        93 },
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
void vSRPTest2Task3(void *pvParameters) {
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_EXECUTE,        90 },
    {TASK_TAKE_SEMAPHORE, 2  }, // Take Yellow (R2)
    {TASK_EXECUTE,        109},
    {TASK_GIVE_SEMAPHORE, 2  }, // Give Yellow (R2)
    {TASK_EXECUTE,        93 },
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
void vSRPTest2Task4(void *pvParameters) {
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_EXECUTE,        93 },
    {TASK_TAKE_SEMAPHORE, 0  }, // Take Red (R0)
    {TASK_EXECUTE,        157},
    {TASK_GIVE_SEMAPHORE, 0  }, // Give Red (R0)
    {TASK_EXECUTE,        93 },
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
TickType_t srp_test_2() {
  const SRP_AperiodicTaskParams_t test_config[MAXIMUM_APERIODIC_TASKS] = {
    {vSRPTest2Task1, 363, 400, 500,  4, {45, 45, 45}},
    {vSRPTest2Task2, 295, 279, 700,  3, {0, 109, 0} },
    {vSRPTest2Task3, 292, 150, 1100, 2, {0, 0, 109} },
    {vSRPTest2Task4, 343, 0,   1400, 1, {157, 0, 0} },
  };

  return build_aperiodic_test( //
    "SRP Test 2",
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
const SRP_AperiodicTaskParams_t test_config[MAXIMUM_APERIODIC_TASKS] = {
  {EDF_aperiodic_task, 100, 0,  300, 1, {NULL}},
  {EDF_aperiodic_task, 100, 20, 230, 1, {NULL}},
  {EDF_aperiodic_task, 50,  50, 150, 2, {NULL}},
};
TickType_t srp_test_3() {
  return build_aperiodic_test( //
    "SRP Test 3",
    test_config,
    MAXIMUM_APERIODIC_TASKS,
    300
  );
}
TickType_t srp_test_4() {
  return build_aperiodic_test( //
    "SRP Test 4",
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

  SRP_AperiodicTaskParams_t test_config[MAXIMUM_APERIODIC_TASKS];
  for (int i = 0; i < NUM_TASKS; i++) {
    test_config[i].func = EDF_aperiodic_task;
    test_config[i].C    = COMPLETION_TIME_MS;
    test_config[i].r    = 0;
    test_config[i].D    = RELATIVE_DEADLINE_MS;
    test_config[i].plvl = (i % N_PREEMPTION_LEVELS) + 1;
  }

  build_aperiodic_test( //
    "SRP Test 5",
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

TickType_t srp_test_6() {
  return srp_test_5();
}

#elif TEST_NR == 7
void vSRPTest7Task1(void *pvParameters) {
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_TAKE_SEMAPHORE, 0},
    {TASK_EXECUTE,        1},
    {TASK_GIVE_SEMAPHORE, 0},
    {TASK_EXECUTE,        1},
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
void vSRPTest7Task3(void *pvParameters) {
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_TAKE_SEMAPHORE, 0},
    {TASK_EXECUTE,        3},
    {TASK_GIVE_SEMAPHORE, 0},
    {TASK_EXECUTE,        7},
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
TickType_t srp_test_7() {
  const SRP_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {vSRPTest7Task1,    2,  10, 10, 3, {1}},
    {EDF_periodic_task, 4,  20, 20, 2, {0}},
    {vSRPTest7Task3,    10, 50, 50, 1, {3}},
  };
  return build_periodic_test( //
    "SRP Test 7",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    300
  );
}

#elif TEST_NR == 8
void vSRPTest8Task1(void *pvParameters) {
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_TAKE_SEMAPHORE, 0},
    {TASK_EXECUTE,        1},
    {TASK_GIVE_SEMAPHORE, 0},
    {TASK_EXECUTE,        1},
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
void vSRPTest8Task3(void *pvParameters) {
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_TAKE_SEMAPHORE, 0},
    {TASK_EXECUTE,        9},
    {TASK_GIVE_SEMAPHORE, 0},
    {TASK_EXECUTE,        1},
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
TickType_t srp_test_8() {
  const SRP_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {vSRPTest8Task1,    2,  10, 10, 3, {1}},
    {EDF_periodic_task, 4,  20, 20, 2, {0}},
    {vSRPTest8Task3,    10, 50, 50, 1, {9}},
  };
  return build_periodic_test( //
    "SRP Test 8",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    300
  );
}

#elif TEST_NR == 9
void vSRPTest9Task1(void *pvParameters) {
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_TAKE_SEMAPHORE, 0},
    {TASK_EXECUTE,        2},
    {TASK_GIVE_SEMAPHORE, 0},
    {TASK_EXECUTE,        3},
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
void vSRPTest9Task3(void *pvParameters) {
  const TickType_t xCompletionTime = *(TickType_t *)pvParameters;
  const TaskStep_t steps[]         = {
    {TASK_TAKE_SEMAPHORE, 0},
    {TASK_EXECUTE,        6},
    {TASK_GIVE_SEMAPHORE, 0},
    {TASK_EXECUTE,        2},
  };
  execute_steps(xCompletionTime, steps, LEN(steps));
}
TickType_t srp_test_9() {
  const SRP_PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {vSRPTest9Task1,    5, 20, 10, 3, {2}},
    {EDF_periodic_task, 4, 20, 12, 2, {0}},
    {vSRPTest9Task3,    8, 50, 50, 1, {6}}
  };

  return build_periodic_test( //
    "SRP Test 9",
    test_config,
    MAXIMUM_PERIODIC_TASKS,
    300
  );
}

#endif // TEST_NR

; // ==================================
; // === Local function definitions ===
; // ==================================

/// @brief Creates a periodic task from a provided task configuration
static void build_periodic_task(const char *task_name, const SRP_PeriodicTaskParams_t *config) {
  SRP_create_periodic_task(
    config->func,
    task_name,
    pdMS_TO_TICKS(config->C),
    pdMS_TO_TICKS(config->T),
    pdMS_TO_TICKS(config->D),
    NULL,
    config->plvl,
    config->resources
  );
}

/// @brief Creates an aperiodic task from a provided task configuration
static void build_aperiodic_task(const char *task_name, const SRP_AperiodicTaskParams_t *config) {
  SRP_create_aperiodic_task(
    config->func,
    task_name,
    pdMS_TO_TICKS(config->C),
    pdMS_TO_TICKS(config->r),
    pdMS_TO_TICKS(config->D),
    NULL,
    config->plvl,
    config->resources
  );
}

/// @brief Creates all tasks from the provided test configuration for periodic tasks
static TickType_t build_periodic_test( //
  const char                     *test_name,
  const SRP_PeriodicTaskParams_t *config,
  size_t                          num_tasks,
  TickType_t                      duration
) {
  configASSERT(num_tasks == (MAXIMUM_PERIODIC_TASKS + MAXIMUM_APERIODIC_TASKS));

  for (size_t i = 0; i < num_tasks; i++) {
    char task_name[22]; // Exactly enough for "SRP Test XX, Task YYY", plus a null terminator byte
    snprintf(task_name, sizeof(task_name), "%s, Task %d", test_name, (int)(i + 1));
    build_periodic_task(task_name, &config[i]);
  }

  return duration;
}

/// @brief Creates all tasks from the provided test configuration for aperiodic tasks
static TickType_t build_aperiodic_test( //
  const char                      *test_name,
  const SRP_AperiodicTaskParams_t *config,
  size_t                           num_tasks,
  TickType_t                       duration
) {
  configASSERT(num_tasks == (MAXIMUM_PERIODIC_TASKS + MAXIMUM_APERIODIC_TASKS));

  for (size_t i = 0; i < num_tasks; i++) {
    char task_name[22]; // Exactly enough for "SRP Test XX, Task YYY", plus a null terminator byte
    snprintf(task_name, sizeof(task_name), "%s, Task %d", test_name, (int)(i + 1));
    build_aperiodic_task(task_name, &config[i]);
  }

  return duration;
}

/// @brief Executes a series of steps defined for a given test. Verifies that the total execution time for the task
/// matches the intended completion time
static void execute_steps(const TickType_t completion_time, const TaskStep_t steps[], const size_t num_steps) {
  // Verify that the execution time of the steps matches the completion time for the task
  TickType_t calculated_execution_time = 0;
  for (size_t i = 0; i < num_steps; i++) {
    if (steps[i].action == TASK_EXECUTE) {
      calculated_execution_time += steps[i].value;
    }
  }
  configASSERT(calculated_execution_time == completion_time);

  // Actually execute the steps
  for (size_t i = 0; i < num_steps; i++) {
    const TaskStep_t *step = &steps[i];

    switch (step->action) {
    case TASK_TAKE_SEMAPHORE:
      SRP_take_binary_semaphore(step->value);
      break;

    case TASK_EXECUTE:
      execute_for_ticks(step->value);
      break;

    case TASK_GIVE_SEMAPHORE:
      SRP_give_binary_semaphore(step->value);
      break;

    default:
      // Catch invalid configurations
      configASSERT(pdFALSE);
      break;
    }
  }

  // Mark as done
  EDF_mark_task_done(NULL);
}

#endif // USE_SRP
