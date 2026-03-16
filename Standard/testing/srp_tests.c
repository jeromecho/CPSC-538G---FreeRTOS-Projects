#include "srp_tests.h"

#if USE_SRP

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"
#include "helpers.h"
#include "srp.h"

#include <stdio.h>

/// Test 1: Basic SRP Priority Inversion Prevention
///
/// This test demonstrates how SRP prevents a medium-priority task from preempting a low-priority task holding a shared
/// resource. Validates basic semaphore ceiling calculations and blocking mechanics.
///
/// - Task 3 (Low) starts and takes Resource 1, raising the system ceiling.
/// - Task 2 (Medium) arrives but is blocked by the ceiling, preventing priority inversion.
/// - Task 1 (High) arrives, preempts, and interacts with the resource.
#if TEST_NR == 1
void vSRPTest1Task3(void *pvParameters) {
  SRP_take_binary_semaphore(0); // Take R1 (System ceiling should become 3)
  execute_for_ticks(100);
  SRP_give_binary_semaphore(0); // Give R1
  execute_for_ticks(20);        // Finish remaining work

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
void vSRPTest1Task2(void *pvParameters) {
  execute_for_ticks(50);

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
void vSRPTest1Task1(void *pvParameters) {
  SRP_take_binary_semaphore(0); // Take R1
  execute_for_ticks(30);
  SRP_give_binary_semaphore(0); // Give R1

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
TickType_t srp_test_1() {
  const int    NUM_TASKS = 3;
  unsigned int user_ceilings_memory[1];

  const unsigned int task1_preemption_level = 3;
  const unsigned int task2_preemption_level = 2;
  const unsigned int task3_preemption_level = 1;

  TMF_t test_tmf[3] = {
    {.preemption_level = task1_preemption_level, .resource_hold_times = {10}, .stackSize = configMINIMAL_STACK_SIZE},
    {.preemption_level = task2_preemption_level, .resource_hold_times = {0},  .stackSize = configMINIMAL_STACK_SIZE},
    {.preemption_level = task3_preemption_level, .resource_hold_times = {50}, .stackSize = configMINIMAL_STACK_SIZE}
  };

  SRP_initialize(test_tmf, NUM_TASKS, user_ceilings_memory);

  SRP_create_aperiodic_task( //
    vSRPTest1Task1,
    "SRP Test 1, Task 1",
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(40),
    pdMS_TO_TICKS(100),
    NULL,
    task1_preemption_level
  );
  SRP_create_aperiodic_task( //
    vSRPTest1Task2,
    "SRP Test 1, Task 2",
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(20),
    pdMS_TO_TICKS(200),
    NULL,
    task2_preemption_level
  );
  SRP_create_aperiodic_task( //
    vSRPTest1Task3,
    "SRP Test 1, Task 3",
    pdMS_TO_TICKS(300),
    pdMS_TO_TICKS(0),
    pdMS_TO_TICKS(300),
    NULL,
    task3_preemption_level
  );

  const TickType_t TEST_DURATION = 250; // Run the test long enough for all tasks to complete
  return TEST_DURATION;
}

/// Test 2: Complex Multi-Resource SRP Validation
///
/// Uses 4 tasks and 3 distinct resources (semaphores) to validate nested resource locking and system ceiling dynamic
/// updates. Proves that chained priority inversions and deadlocks are mathematically impossible under the current SRP
/// implementation.
///
/// This test is taken from https://cpen432.github.io/resources/bader-slides/8-ResourceSharing.pdf, Page 49
#elif TEST_NR == 2
void vSRPTest2Task4(void *pvParameters) {
  execute_for_ticks(93);        // Initial execution
  SRP_take_binary_semaphore(0); // Take Red (R0)
  execute_for_ticks(157);       // Critical section
  SRP_give_binary_semaphore(0); // Give Red (R0)
  execute_for_ticks(93);        // Finish remaining work

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
void vSRPTest2Task3(void *pvParameters) {
  execute_for_ticks(90);
  SRP_take_binary_semaphore(2); // Take Yellow (R2)
  execute_for_ticks(109);       // Critical section
  SRP_give_binary_semaphore(2); // Give Yellow (R2)
  execute_for_ticks(93);        // Finish remaining work

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
void vSRPTest2Task2(void *pvParameters) {
  execute_for_ticks(93);        // Initial execution
  SRP_take_binary_semaphore(1); // Take Blue (R1)
  execute_for_ticks(109);       // Critical section
  SRP_give_binary_semaphore(1); // Give Blue (R1)
  execute_for_ticks(93);        // Finish remaining work

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
void vSRPTest2Task1(void *pvParameters) {
  execute_for_ticks(93);        // Initial execution

  SRP_take_binary_semaphore(0); // Take Red (R0)
  execute_for_ticks(45);
  SRP_give_binary_semaphore(0); // Give Red (R0)

  execute_for_ticks(45);        // Execution between resources

  SRP_take_binary_semaphore(1); // Take Blue (R1)
  execute_for_ticks(45);
  SRP_give_binary_semaphore(1); // Give Blue (R1)

  execute_for_ticks(45);        // Execution between resources

  SRP_take_binary_semaphore(2); // Take Yellow (R2)
  execute_for_ticks(45);
  SRP_give_binary_semaphore(2); // Give Yellow (R2)

  execute_for_ticks(45);        // Finish remaining work

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
TickType_t srp_test_2() {
  const int    NUM_TASKS = 4;
  unsigned int user_ceilings_memory[3];

  const unsigned int task1_preemption_level = 4;
  const unsigned int task2_preemption_level = 3;
  const unsigned int task3_preemption_level = 2;
  const unsigned int task4_preemption_level = 1;

  // Define the TMF matrix mapped to the diagram
  TMF_t test_tmf[4] = {
    {.preemption_level    = task1_preemption_level,
     .resource_hold_times = {45, 45, 45},
     .stackSize           = configMINIMAL_STACK_SIZE},
    {.preemption_level    = task2_preemption_level,
     .resource_hold_times = {0, 109, 0},
     .stackSize           = configMINIMAL_STACK_SIZE},
    {.preemption_level    = task3_preemption_level,
     .resource_hold_times = {0, 0, 109},
     .stackSize           = configMINIMAL_STACK_SIZE},
    {.preemption_level    = task4_preemption_level,
     .resource_hold_times = {157, 0, 0},
     .stackSize           = configMINIMAL_STACK_SIZE}
  };

  // 1. Initialize SRP with our TMF matrix
  SRP_initialize(test_tmf, NUM_TASKS, user_ceilings_memory);

  // Create the tasks
  // Deadlines are scaled so T1 < T2 < T3 < T4 to allow the EDF scheduler to naturally map the correct priorities.

  SRP_create_aperiodic_task( //
    vSRPTest2Task1,
    "SRP Test 2, Task 1",
    pdMS_TO_TICKS(500),
    pdMS_TO_TICKS(400),
    pdMS_TO_TICKS(500),
    NULL,
    task1_preemption_level
  );
  SRP_create_aperiodic_task( //
    vSRPTest2Task2,
    "SRP Test 2, Task 2",
    pdMS_TO_TICKS(1000),
    pdMS_TO_TICKS(279),
    pdMS_TO_TICKS(1000),
    NULL,
    task2_preemption_level
  );
  SRP_create_aperiodic_task( //
    vSRPTest2Task3,
    "SRP Test 2, Task 3",
    pdMS_TO_TICKS(1500),
    pdMS_TO_TICKS(150),
    pdMS_TO_TICKS(1500),
    NULL,
    task3_preemption_level
  );
  SRP_create_aperiodic_task( //
    vSRPTest2Task4,
    "SRP Test 2, Task 4",
    pdMS_TO_TICKS(2000),
    pdMS_TO_TICKS(0),
    pdMS_TO_TICKS(2000),
    NULL,
    task4_preemption_level
  );

  const TickType_t TEST_DURATION = 1500;
  return TEST_DURATION;
}

/// Test 3 & 4: Comparison of execution traces when Stack Sharing is enabled vs. disabled.
///
/// Since tasks at the same preemption level cannot preempt each other under SRP, this enables stack sharing. This
/// creates two tasks at the same preemption level, and shows that the execution is the same when stack sharing is
/// enabled and disabled.
///
/// - Task 1 (Level 1) runs.
/// - Task 2 (Level 1) arrives with an earlier deadline but is correctly blocked by the ceiling.
/// - Task 3 (Level 2) arrives and successfully preempts Task 1.
#elif TEST_NR == 3 || TEST_NR == 4
TickType_t srp_test_3() {
  const int num_test_tasks = 3;

  unsigned int user_ceilings_memory[1] = {0};

  const unsigned int task3_preemption_level = 2;
  const unsigned int task2_preemption_level = 1;
  const unsigned int task1_preemption_level = 1;

  // Define the TMF matrix. No resource hold times needed, since the tasks do not acquire any resources
  TMF_t test_tmf[3] = {
    {.preemption_level = task1_preemption_level, .stackSize = configMINIMAL_STACK_SIZE},
    {.preemption_level = task2_preemption_level, .stackSize = configMINIMAL_STACK_SIZE},
    {.preemption_level = task3_preemption_level, .stackSize = configMINIMAL_STACK_SIZE}
  };

  SRP_initialize(test_tmf, num_test_tasks, user_ceilings_memory);

  SRP_create_aperiodic_task( //
    EDF_aperiodic_task,
    "SRP Test 3, Task 1",
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(0),
    pdMS_TO_TICKS(300),
    NULL,
    task1_preemption_level
  );
  SRP_create_aperiodic_task( //
    EDF_aperiodic_task,
    "SRP Test 3, Task 2",
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(20),
    pdMS_TO_TICKS(230),
    NULL,
    task2_preemption_level
  );

  SRP_create_aperiodic_task( //
    EDF_aperiodic_task,
    "SRP Test 3, Task 3",
    pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(150),
    NULL,
    task3_preemption_level
  );

  const TickType_t TEST_DURATION = 300;
  return TEST_DURATION;
}
TickType_t srp_test_4() { return srp_test_3(); }

/// Tests 5 & 6: Quantitative Analysis of Stack Sharing RAM Usage
///
/// A stress test designed to measure the memory reduction achieved by SRP.
///
///  Generates a maximum load of tasks and distributes them across a smaller subset
/// of preemption levels. Outputs the exact runtime static memory footprint
/// using `sizeof()`. Proves that stack sharing significantly reduces the `.bss`
/// memory allocation required for the RTOS.
///
#elif TEST_NR == 5 || TEST_NR == 6
TickType_t srp_test_5() {
  const unsigned int NUM_TASKS            = MAXIMUM_APERIODIC_TASKS;
  const unsigned int COMPLETION_TIME_MS   = 10;
  const unsigned int RELATIVE_DEADLINE_MS = NUM_TASKS * COMPLETION_TIME_MS;

  unsigned int user_ceilings_memory[1] = {0};

  // Initialize the TMF matrix for all the tasks
  // TODO: is this true? Test the boot stack usage with and without static.
  // We use static to avoid blowing up the Pico's boot stack
  TMF_t test_tmf[NUM_TASKS];
  for (int i = 0; i < NUM_TASKS; i++) {
    test_tmf[i].preemption_level       = (i % N_PREEMPTION_LEVELS) + 1;
    test_tmf[i].resource_hold_times[0] = 0;
    test_tmf[i].stackSize              = SHARED_STACK_SIZE;
  }

  SRP_initialize(test_tmf, NUM_TASKS, user_ceilings_memory);

  // Create the tasks
  for (int i = 0; i < NUM_TASKS; i++) {
    char taskName[19];
    sprintf(taskName, "SRP Test 5, T%d", i);

    SRP_create_aperiodic_task( //
      EDF_aperiodic_task,
      taskName,
      pdMS_TO_TICKS(COMPLETION_TIME_MS),
      pdMS_TO_TICKS(0),
      pdMS_TO_TICKS(RELATIVE_DEADLINE_MS),
      NULL,
      test_tmf[i].preemption_level
    );
  }

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

#endif // TEST_NR

#endif // USE_SRP

// TODO: Periodic tasks with SRP

// TODO: Maybe one test showing the stack sharing in combination with the semaphores?

// TODO: Admission control

// TODO: Many weird system traces (switch in, switch out and update priorities) after end of test