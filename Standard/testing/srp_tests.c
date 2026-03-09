#include "srp_tests.h"

#if USE_SRP

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"
#include "helpers.h"
#include "srp.h"

// === Test 1: Basic SRP Functionality Test ===
void vSRPTest1Task3(void *pvParameters) {
  SRP_take_binary_semaphore(0); // Take R1 (System ceiling should become 3)
  execute_for_ticks(100);
  SRP_give_binary_semaphore(0); // Give R1
  execute_for_ticks(20);        // Finish remaining work

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
void vSRPTest1Task2(void *pvParameters) {
  // printf("System ceiling: %u\n", get_srp_system_ceiling());
  execute_for_ticks(50);

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
void vSRPTest1Task1(void *pvParameters) {
  // printf("System ceiling: %u\n", get_srp_system_ceiling());
  SRP_take_binary_semaphore(0); // Take R1
  execute_for_ticks(30);
  // printf("System ceiling: %u\n", get_srp_system_ceiling());
  SRP_give_binary_semaphore(0); // Give R1

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
TickType_t srp_test_1() {
  // 1 Resource (R1), 3 Tasks
  const int    num_test_tasks = 3;
  unsigned int user_ceilings_memory[1]; // For static allocation

  // Define the TMF matrix according to your design document
  TMF_t test_tmf[3] = {
    // Task 1 (High Priority - Level 3). Needs R1.
    {.preemption_level = 3, .resource_hold_times = {10}, .stackSize = configMINIMAL_STACK_SIZE},

    // Task 2 (Medium Priority - Level 2). Does NOT need R1.
    {.preemption_level = 2, .resource_hold_times = {0},  .stackSize = configMINIMAL_STACK_SIZE},

    // Task 3 (Low Priority - Level 1). Needs R1.
    {.preemption_level = 1, .resource_hold_times = {50}, .stackSize = configMINIMAL_STACK_SIZE}
  };

  // 1. Initialize SRP with our TMF matrix
  SRP_initialize(test_tmf, num_test_tasks, user_ceilings_memory);

  // 2. Create the tasks using your Aperiodic creation function
  // (Assuming you've modified xTaskCreateAperiodic or your TMB struct to accept preemption levels)
  TaskHandle_t t1, t2, t3;

  // Note: You might need to adjust this depending on how you added preemptionLevel to your structs.
  // For now, we rely on the EDF scheduler prioritizing the shortest absolute deadline.

  // Task 1: Level 3 (Highest). Deadline = 100.
  EDF_create_aperiodic_task( //
    vSRPTest1Task1,
    "Task1",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(40),
    pdMS_TO_TICKS(100),
    &t1
  );
  aperiodic_tasks[0].preemption_level = 3;

  // Task 2: Level 2 (Medium). Deadline = 200.
  EDF_create_aperiodic_task( //
    vSRPTest1Task2,
    "Task2",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(20),
    pdMS_TO_TICKS(200),
    &t2
  );
  aperiodic_tasks[1].preemption_level = 2;

  // // Task 3: Level 1 (Lowest). Deadline = 300.
  EDF_create_aperiodic_task( //
    vSRPTest1Task3,
    "Task3",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(300),
    pdMS_TO_TICKS(0),
    pdMS_TO_TICKS(300),
    &t3
  );
  aperiodic_tasks[2].preemption_level = 1;

  TickType_t test_duration = 3000; // Run the test long enough for all tasks to complete
  return test_duration;
}

// === Test 2: Basic SRP Functionality Test with Multiple Resources ===
void vSRPTest2Task4(void *pvParameters) {
  execute_for_ticks(93);        // Initial execution
  SRP_take_binary_semaphore(0); // Take Red (R0) - ceiling becomes 4
  execute_for_ticks(157);       // Critical section
  SRP_give_binary_semaphore(0); // Give Red (R0)
  execute_for_ticks(93);        // Finish remaining work

  EDF_mark_task_done(xTaskGetCurrentTaskHandle());
}
void vSRPTest2Task3(void *pvParameters) {
  execute_for_ticks(90);        // 29 ticks before T2 arrives + 61 ticks after T2 finishes
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

// This test is taken from https://cpen432.github.io/resources/bader-slides/8-ResourceSharing.pdf
// Page 49
TickType_t srp_test_2() {
  const int    num_test_tasks = 4;
  unsigned int user_ceilings_memory[3];

  // Define the TMF matrix mapped to the diagram
  TMF_t test_tmf[4] = {
    // Task 1 (Level 4). Needs R0, R1, R2.
    {.preemption_level = 4, .resource_hold_times = {45, 45, 45}, .stackSize = configMINIMAL_STACK_SIZE},

    // Task 2 (Level 3). Needs R1 (Blue).
    {.preemption_level = 3, .resource_hold_times = {0, 109, 0},  .stackSize = configMINIMAL_STACK_SIZE},

    // Task 3 (Level 2). Needs R2 (Yellow).
    {.preemption_level = 2, .resource_hold_times = {0, 0, 109},  .stackSize = configMINIMAL_STACK_SIZE},

    // Task 4 (Level 1). Needs R0 (Red).
    {.preemption_level = 1, .resource_hold_times = {157, 0, 0},  .stackSize = configMINIMAL_STACK_SIZE}
  };

  // 1. Initialize SRP with our TMF matrix
  SRP_initialize(test_tmf, num_test_tasks, user_ceilings_memory);

  TaskHandle_t t1, t2, t3, t4;

  // 2. Create the tasks
  // Deadlines are scaled so T1 < T2 < T3 < T4 to allow the EDF scheduler to naturally map the correct priorities.

  // Task 1: Level 4 (Highest). Arrives at t=400 (Middle of T2's Blue segment).
  EDF_create_aperiodic_task( //
    vSRPTest2Task1,
    "Task1",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(500),
    pdMS_TO_TICKS(400),
    pdMS_TO_TICKS(500),
    &t1
  );
  aperiodic_tasks[0].preemption_level = 4;

  // Task 2: Level 3 (Medium-High). Arrives at t=279 (End of T3's first cyan segment).
  EDF_create_aperiodic_task( //
    vSRPTest2Task2,
    "Task2",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(1000),
    pdMS_TO_TICKS(279),
    pdMS_TO_TICKS(1000),
    &t2
  );
  aperiodic_tasks[1].preemption_level = 3;

  // Task 3: Level 2 (Medium-Low). Arrives at t=150 (Middle of T4's Red segment).
  EDF_create_aperiodic_task( //
    vSRPTest2Task3,
    "Task3",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(1500),
    pdMS_TO_TICKS(150),
    pdMS_TO_TICKS(1500),
    &t3
  );
  aperiodic_tasks[2].preemption_level = 2;

  // Task 4: Level 1 (Lowest). Arrives at t=0.
  EDF_create_aperiodic_task( //
    vSRPTest2Task4,
    "Task4",
    configMINIMAL_STACK_SIZE,
    pdMS_TO_TICKS(2000),
    pdMS_TO_TICKS(0),
    pdMS_TO_TICKS(2000),
    &t4
  );
  aperiodic_tasks[3].preemption_level = 1;

  const TickType_t TEST_DURATION = 1300; // Run the test long enough for all tasks to complete
  return TEST_DURATION;
}


#endif // USE_SRP
