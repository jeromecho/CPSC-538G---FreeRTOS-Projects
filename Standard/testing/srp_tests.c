#include "ProjectConfig.h"

#if USE_SRP

#include "srp_tests.h"
#include "testing.h"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"
#include "helpers.h"

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
  const TaskStep_t steps[] = {
    {1,  TASK_TAKE_SEMAPHORE, 0},
    {30, TASK_GIVE_SEMAPHORE, 0},
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void vSRPTest1Task3(void *pvParameters) {
  const TaskStep_t steps[] = {
    {1,   TASK_TAKE_SEMAPHORE, 0},
    {100, TASK_GIVE_SEMAPHORE, 0},
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void srp_test_1() {
  const AperiodicTaskParams_t test_config[MAXIMUM_APERIODIC_TASKS] = {
    {vSRPTest1Task1,     30,  40, 100, 3, {30} },
    {EDF_aperiodic_task, 50,  20, 200, 2, {0}  },
    {vSRPTest1Task3,     120, 0,  300, 1, {100}},
  };

  build_aperiodic_test( //
    "SRP Test 1",
    test_config,
    MAXIMUM_APERIODIC_TASKS
  );
}
#endif

#if TEST_NR == 2
/// Test 2: Complex Multi-Resource SRP Validation
///
/// Uses 4 tasks and 3 distinct resources (semaphores) to validate nested resource locking and system ceiling dynamic
/// updates. Proves that chained priority inversions and deadlocks are mathematically impossible under the current SRP
/// implementation.
///
/// This test is taken from https://cpen432.github.io/resources/bader-slides/8-ResourceSharing.pdf, Page 49
void vSRPTest2Task1(void *pvParameters) {
  const TaskStep_t steps[] = {
    {93,  TASK_TAKE_SEMAPHORE, 0}, // Take Red (R0)
    {138, TASK_GIVE_SEMAPHORE, 0}, // Give Red (R0)
    {183, TASK_TAKE_SEMAPHORE, 1}, // Take Blue (R1)
    {228, TASK_GIVE_SEMAPHORE, 1}, // Give Blue (R1)
    {273, TASK_TAKE_SEMAPHORE, 2}, // Take Yellow (R2)
    {318, TASK_GIVE_SEMAPHORE, 2}, // Give Yellow (R2)
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void vSRPTest2Task2(void *pvParameters) {
  const TaskStep_t steps[] = {
    {93,  TASK_TAKE_SEMAPHORE, 1}, // Take Blue (R1)
    {202, TASK_GIVE_SEMAPHORE, 1}, // Give Blue (R1)
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void vSRPTest2Task3(void *pvParameters) {
  const TaskStep_t steps[] = {
    {90,  TASK_TAKE_SEMAPHORE, 2}, // Take Yellow (R2)
    {199, TASK_GIVE_SEMAPHORE, 2}, // Give Yellow (R2)
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void vSRPTest2Task4(void *pvParameters) {
  const TaskStep_t steps[] = {
    {93,  TASK_TAKE_SEMAPHORE, 0}, // Take Red (R0)
    {250, TASK_GIVE_SEMAPHORE, 0}, // Give Red (R0)
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void srp_test_2() {
  const AperiodicTaskParams_t test_config[MAXIMUM_APERIODIC_TASKS] = {
    {vSRPTest2Task1, 363, 400, 500,  4, {45, 45, 45}},
    {vSRPTest2Task2, 295, 279, 700,  3, {0, 109, 0} },
    {vSRPTest2Task3, 292, 150, 1100, 2, {0, 0, 109} },
    {vSRPTest2Task4, 343, 0,   1400, 1, {157, 0, 0} },
  };

  build_aperiodic_test( //
    "SRP Test 2",
    test_config,
    MAXIMUM_APERIODIC_TASKS
  );
}
#endif

#if TEST_NR == 3 || TEST_NR == 4
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
  {EDF_aperiodic_task, 100, 0,  300, 1, {}},
  {EDF_aperiodic_task, 100, 20, 230, 1, {}},
  {EDF_aperiodic_task, 50,  50, 150, 2, {}},
};
void srp_test_3() {
  build_aperiodic_test( //
    "SRP Test 3",
    test_config,
    MAXIMUM_APERIODIC_TASKS
  );
}
void srp_test_4() {
  build_aperiodic_test( //
    "SRP Test 4",
    test_config,
    MAXIMUM_APERIODIC_TASKS
  );
}
#endif

#if TEST_NR == 5 || TEST_NR == 6
/// Tests 5 & 6: Quantitative Analysis of Stack Sharing RAM Usage
///
/// A stress test designed to measure the memory reduction achieved by SRP.
///
///  Generates a maximum load of tasks and distributes them across a smaller subset
/// of preemption levels. Outputs the exact runtime static memory footprint
/// using `sizeof()`. Proves that stack sharing significantly reduces the `.bss`
/// memory allocation required for the RTOS.
///
void srp_test_5() {
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

  build_aperiodic_test( //
    "SRP Test 5",
    test_config,
    NUM_TASKS
  );
}

void srp_test_6() { srp_test_5(); }
#endif

#if TEST_NR == 7
void vSRPTest7Task1(void *pvParameters) {
  const TaskStep_t steps[] = {
    {1, TASK_TAKE_SEMAPHORE, 0},
    {2, TASK_GIVE_SEMAPHORE, 0},
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void vSRPTest7Task3(void *pvParameters) {
  const TaskStep_t steps[] = {
    {1, TASK_TAKE_SEMAPHORE, 0},
    {4, TASK_GIVE_SEMAPHORE, 0},
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void srp_test_7() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {vSRPTest7Task1,    2,  10, 10, 3, {1}},
    {EDF_periodic_task, 4,  20, 20, 2, {0}},
    {vSRPTest7Task3,    10, 50, 50, 1, {3}},
  };
  build_periodic_test( //
    "SRP Test 7",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}
#endif

#if TEST_NR == 8
void vSRPTest8Task1(void *pvParameters) {
  const TaskStep_t steps[] = {
    {1, TASK_TAKE_SEMAPHORE, 0},
    {2, TASK_GIVE_SEMAPHORE, 0},
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void vSRPTest8Task3(void *pvParameters) {
  const TaskStep_t steps[] = {
    {1,  TASK_TAKE_SEMAPHORE, 0},
    {10, TASK_GIVE_SEMAPHORE, 0},
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void srp_test_8() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {vSRPTest8Task1,    2,  10, 10, 3, {1}},
    {EDF_periodic_task, 4,  20, 20, 2, {0}},
    {vSRPTest8Task3,    10, 50, 50, 1, {9}},
  };
  build_periodic_test( //
    "SRP Test 8",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}
#endif

#if TEST_NR == 9
void vSRPTest9Task1(void *pvParameters) {
  const TaskStep_t steps[] = {
    {1, TASK_TAKE_SEMAPHORE, 0},
    {3, TASK_GIVE_SEMAPHORE, 0},
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void vSRPTest9Task3(void *pvParameters) {
  const TaskStep_t steps[] = {
    {1, TASK_TAKE_SEMAPHORE, 0},
    {7, TASK_GIVE_SEMAPHORE, 0},
  };
  EXECUTE_WORKLOAD(steps, (TickType_t)pvParameters);
}
void srp_test_9() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {vSRPTest9Task1,    5, 20, 10, 3, {2}},
    {EDF_periodic_task, 4, 20, 12, 2, {0}},
    {vSRPTest9Task3,    8, 50, 50, 1, {6}}
  };

  build_periodic_test( //
    "SRP Test 9",
    test_config,
    MAXIMUM_PERIODIC_TASKS
  );
}

#endif // TEST_NR

#endif // USE_SRP
