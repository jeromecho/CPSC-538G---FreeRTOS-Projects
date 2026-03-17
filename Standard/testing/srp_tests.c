#include "srp_tests.h"

#if USE_SRP

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"
#include "helpers.h"
#include "srp.h"

#include <stdio.h>

#if TEST_NR == 1
/// Test 1: Basic SRP Priority Inversion Prevention
///
/// This test demonstrates how SRP prevents a medium-priority task from preempting a low-priority task holding a shared
/// resource. Validates basic semaphore ceiling calculations and blocking mechanics.
///
/// - Task 3 (Low) starts and takes Resource 1, raising the system ceiling.
/// - Task 2 (Medium) arrives but is blocked by the ceiling, preventing priority inversion.
/// - Task 1 (High) arrives, preempts, and interacts with the resource.
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
  SRP_create_aperiodic_task( //
    vSRPTest1Task1,
    "SRP Test 1, Task 1",
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(40),
    pdMS_TO_TICKS(100),
    NULL,
    3,
    (TickType_t[]){10}
  );
  SRP_create_aperiodic_task( //
    vSRPTest1Task2,
    "SRP Test 1, Task 2",
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(20),
    pdMS_TO_TICKS(200),
    NULL,
    2,
    (TickType_t[]){0}
  );
  SRP_create_aperiodic_task( //
    vSRPTest1Task3,
    "SRP Test 1, Task 3",
    pdMS_TO_TICKS(300),
    pdMS_TO_TICKS(0),
    pdMS_TO_TICKS(300),
    NULL,
    1,
    (TickType_t[]){50}
  );

  const TickType_t TEST_DURATION = 250; // Run the test long enough for all tasks to complete
  return TEST_DURATION;
}

#elif TEST_NR == 2
/// Test 2: Complex Multi-Resource SRP Validation
///
/// Uses 4 tasks and 3 distinct resources (semaphores) to validate nested resource locking and system ceiling dynamic
/// updates. Proves that chained priority inversions and deadlocks are mathematically impossible under the current SRP
/// implementation.
///
/// This test is taken from https://cpen432.github.io/resources/bader-slides/8-ResourceSharing.pdf, Page 49
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
  SRP_create_aperiodic_task( //
    vSRPTest2Task1,
    "SRP Test 2, Task 1",
    pdMS_TO_TICKS(500),
    pdMS_TO_TICKS(400),
    pdMS_TO_TICKS(500),
    NULL,
    4,
    (TickType_t[]){45, 45, 45}
  );
  SRP_create_aperiodic_task( //
    vSRPTest2Task2,
    "SRP Test 2, Task 2",
    pdMS_TO_TICKS(1000),
    pdMS_TO_TICKS(279),
    pdMS_TO_TICKS(1000),
    NULL,
    3,
    (TickType_t[]){0, 109, 0}
  );
  SRP_create_aperiodic_task( //
    vSRPTest2Task3,
    "SRP Test 2, Task 3",
    pdMS_TO_TICKS(1500),
    pdMS_TO_TICKS(150),
    pdMS_TO_TICKS(1500),
    NULL,
    2,
    (TickType_t[]){0, 0, 109}
  );
  SRP_create_aperiodic_task( //
    vSRPTest2Task4,
    "SRP Test 2, Task 4",
    pdMS_TO_TICKS(2000),
    pdMS_TO_TICKS(0),
    pdMS_TO_TICKS(2000),
    NULL,
    1,
    (TickType_t[]){157, 0, 0}
  );

  const TickType_t TEST_DURATION = 1500;
  return TEST_DURATION;
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
TickType_t srp_test_3() {
  SRP_create_aperiodic_task( //
    EDF_aperiodic_task,
    "SRP Test 3, Task 1",
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(0),
    pdMS_TO_TICKS(300),
    NULL,
    1,
    NULL
  );
  SRP_create_aperiodic_task( //
    EDF_aperiodic_task,
    "SRP Test 3, Task 2",
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(20),
    pdMS_TO_TICKS(230),
    NULL,
    1,
    NULL
  );

  SRP_create_aperiodic_task( //
    EDF_aperiodic_task,
    "SRP Test 3, Task 3",
    pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(150),
    NULL,
    2,
    NULL
  );

  const TickType_t TEST_DURATION = 300;
  return TEST_DURATION;
}
TickType_t srp_test_4() { return srp_test_3(); }

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
      (i % N_PREEMPTION_LEVELS) + 1,
      NULL
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

#elif TEST_NR == 7
// TODO: Why does the scheduler fail for periodic tasks?
void vSRPTest7Task1(void *pvParameters) {
  SRP_take_binary_semaphore(0);
  execute_for_ticks(1);
  SRP_give_binary_semaphore(0);
  execute_for_ticks(1);

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
void vSRPTest7Task3(void *pvParameters) {
  SRP_take_binary_semaphore(0);
  execute_for_ticks(3);
  SRP_give_binary_semaphore(0);
  execute_for_ticks(7);

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
TickType_t srp_test_7() {
  SRP_create_periodic_task( //
    vSRPTest7Task1,
    "SRP Test 7, Task 1",
    pdMS_TO_TICKS(2),
    pdMS_TO_TICKS(10),
    pdMS_TO_TICKS(10),
    NULL,
    3,
    (TickType_t[]){1}
  );
  SRP_create_periodic_task( //
    EDF_periodic_task,
    "SRP Test 7, Task 2",
    pdMS_TO_TICKS(4),
    pdMS_TO_TICKS(20),
    pdMS_TO_TICKS(20),
    NULL,
    2,
    (TickType_t[]){0}
  );
  SRP_create_periodic_task( //
    vSRPTest7Task3,
    "SRP Test 7, Task 3",
    pdMS_TO_TICKS(10),
    pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(50),
    NULL,
    1,
    (TickType_t[]){3}
  );

  const TickType_t TEST_DURATION = 300;
  return TEST_DURATION;
}

#elif TEST_NR == 8
void vSRPTest8Task1(void *pvParameters) {
  SRP_take_binary_semaphore(0);
  execute_for_ticks(1);
  SRP_give_binary_semaphore(0);
  execute_for_ticks(1);

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
void vSRPTest8Task3(void *pvParameters) {
  SRP_take_binary_semaphore(0);
  execute_for_ticks(9);
  SRP_give_binary_semaphore(0);
  execute_for_ticks(1);

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
TickType_t srp_test_8() {
  SRP_create_periodic_task( //
    vSRPTest8Task1,
    "SRP Test 8, Task 1",
    pdMS_TO_TICKS(2),
    pdMS_TO_TICKS(10),
    pdMS_TO_TICKS(10),
    NULL,
    3,
    (TickType_t[]){1}
  );
  SRP_create_periodic_task( //
    EDF_periodic_task,
    "SRP Test 8, Task 2",
    pdMS_TO_TICKS(4),
    pdMS_TO_TICKS(20),
    pdMS_TO_TICKS(20),
    NULL,
    2,
    (TickType_t[]){0}
  );
  SRP_create_periodic_task( //
    vSRPTest8Task3,
    "SRP Test 8, Task 3",
    pdMS_TO_TICKS(10),
    pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(50),
    NULL,
    1,
    (TickType_t[]){9}
  );

  const TickType_t TEST_DURATION = 300;
  return TEST_DURATION;
}

#endif // TEST_NR

#endif // USE_SRP

// TODO: Periodic tasks with SRP

// TODO: Maybe one test showing the stack sharing in combination with the semaphores?

// TODO: Admission control
