#include "srp_tests.h"

#include "FreeRTOS.h"
#include "edf_scheduler.h"
#include "helpers.h"
#include "srp.h"

#include <stdio.h>

// Task 3: Lowest Priority (Level 1)
void vTestTask3(void *pvParameters) {
  vBinSempahoreTakeSRP(0); // Take R1 (System ceiling becomes 3)
  busy_wait_ticks(100);
  vBinSemaphoreGiveSRP(0); // Give R1
  busy_wait_ticks(20);     // Finish remaining work
  vTaskDelete(NULL);
}

// Task 2: Medium Priority (Level 2)
void vTestTask2(void *pvParameters) {
  busy_wait_ticks(50);
  vTaskDelete(NULL);
}

// Task 1: Highest Priority (Level 3)
void vTestTask1(void *pvParameters) {
  vBinSempahoreTakeSRP(0); // Take R1
  busy_wait_ticks(30);
  vBinSemaphoreGiveSRP(0); // Give R1
  vTaskDelete(NULL);
}

void srp_test_1() {
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
  vSRP_Initialize(test_tmf, num_test_tasks, user_ceilings_memory);

  // 2. Create the tasks using your Aperiodic creation function
  // (Assuming you've modified xTaskCreateAperiodic or your TMB struct to accept preemption levels)
  TaskHandle_t t1, t2, t3;

  // Note: You might need to adjust this depending on how you added preemptionLevel to your structs.
  // For now, we rely on the EDF scheduler prioritizing the shortest absolute deadline.

  // Task 1: Level 3 (Highest). Deadline = 100.
  xTaskCreateAperiodic(vTestTask1, "Task1", configMINIMAL_STACK_SIZE, (void *)100, 40, 100, &t1);
  aperiodic_tasks[0].tmb.preemption_level = 3;

  // Task 2: Level 2 (Medium). Deadline = 200.
  xTaskCreateAperiodic(vTestTask2, "Task2", configMINIMAL_STACK_SIZE, (void *)200, 20, 200, &t2);
  aperiodic_tasks[1].tmb.preemption_level = 2;

  // // Task 3: Level 1 (Lowest). Deadline = 300.
  xTaskCreateAperiodic(vTestTask3, "Task3", configMINIMAL_STACK_SIZE, (void *)300, 0, 300, &t3);
  aperiodic_tasks[2].tmb.preemption_level = 1;
}
