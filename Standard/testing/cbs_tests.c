#include "cbs_tests.h"

#if USE_CBS
// TODO: EDF test 5 needs updated logic for the admission (u=1)

#include "FreeRTOS.h" // IWYU pragma: keep
#include "cbs.h"
#include "edf_scheduler.h"
#include "helpers.h"
#include <stdio.h>

// ======= FUNCTION MACROS ========
// ================================

#define GENERATE_APERIODIC_TASK(name, ms)                                                                              \
  BaseType_t CBS_task_##name(void) {                                                                                   \
    execute_for_ticks(pdMS_TO_TICKS(ms));                                                                              \
    return pdTRUE;                                                                                                     \
  }

GENERATE_APERIODIC_TASK(4, 4)
GENERATE_APERIODIC_TASK(3, 3)
GENERATE_APERIODIC_TASK(2, 2)
GENERATE_APERIODIC_TASK(1, 1)

// ========== CONSTANTS ===========
// ================================

const int CBS_SERVER1_ID = 1;
const int CBS_SERVER2_ID = 2;
const int CBS_SERVER3_ID = 3;

// Wrapper over periodic task
BaseType_t platform_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
) {
#if USE_EDF
  return EDF_create_periodic_task(task_function, task_name, completion_time, period, relative_deadline, TMB_handle);
#else
#error "No scheduler implementation defined in `create_periodic_task`"
  return pdFAIL;
#endif
};

; // ====================================
; // === Tests for Base Functionality ===
; // ====================================

// Smoke test #1 (textbook pg.190): 1 periodic task with 2 aperiodic tasks; 1 CBS server
TickType_t cbs_test_1() {
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(3));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(13));
  return pdMS_TO_TICKS(21);
}

// Single aperiodic task running on CBS server
TickType_t cbs_test_2() {
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, 0);
  return pdMS_TO_TICKS(10);
}

// Multiple tasks queueing up to max capacity on 1 CBS server
TickType_t cbs_test_3() {
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  for (size_t i = 0; i < CBS_QUEUE_CAPACITY; i++) {
    CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, 0);
  }
  return pdMS_TO_TICKS(50);
}

// Smoke Test #2: Different setup with 1 periodic task and 1 CBS server
TickType_t cbs_test_4() {
  create_cbs_server(pdMS_TO_TICKS(2), pdMS_TO_TICKS(10), CBS_SERVER1_ID);
  platform_create_periodic_task(EDF_periodic_task, "P1", pdMS_TO_TICKS(2), pdMS_TO_TICKS(5), pdMS_TO_TICKS(4), NULL);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, 0);
  return pdMS_TO_TICKS(30);
}

// Smoke Test #3: Multiple Periodic Tasks Running Alongside Single CBS Server
TickType_t cbs_test_5() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(10), CBS_SERVER1_ID);
  platform_create_periodic_task(EDF_periodic_task, "P1", pdMS_TO_TICKS(2), pdMS_TO_TICKS(4), pdMS_TO_TICKS(4), NULL);
  platform_create_periodic_task(EDF_periodic_task, "P2", pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), pdMS_TO_TICKS(8), NULL);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, 0);
  return pdMS_TO_TICKS(50);
}

// Smoke Test #4: Multiple Periodic Tasks Running Alongside 2 symmetric CBS servers
TickType_t cbs_test_6() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(2), pdMS_TO_TICKS(6), pdMS_TO_TICKS(6), NULL
  );
  platform_create_periodic_task(
    EDF_periodic_task, "Task P2", pdMS_TO_TICKS(1), pdMS_TO_TICKS(3), pdMS_TO_TICKS(3), NULL
  );
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER2_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER2_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID, pdMS_TO_TICKS(40));
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER2_ID, pdMS_TO_TICKS(40));
  return pdMS_TO_TICKS(100);
}

// Smoke Test #4: Multiple Periodic Tasks Running Alongside 2 asymmetric CBS servers
TickType_t cbs_test_7() {
  create_cbs_server(pdMS_TO_TICKS(2), pdMS_TO_TICKS(16), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(4), pdMS_TO_TICKS(20), CBS_SERVER2_ID);

  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(12), pdMS_TO_TICKS(12), NULL
  );
  platform_create_periodic_task(
    EDF_periodic_task, "Task P2", pdMS_TO_TICKS(3), pdMS_TO_TICKS(9), pdMS_TO_TICKS(9), NULL
  );
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER2_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER2_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID, pdMS_TO_TICKS(40));
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER2_ID, pdMS_TO_TICKS(40));
  return pdMS_TO_TICKS(200);
}

// Multiple (2) symmetric CBS servers in isolation
TickType_t cbs_test_8() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER2_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER2_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID, pdMS_TO_TICKS(40));
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER2_ID, pdMS_TO_TICKS(40));
  return pdMS_TO_TICKS(50);
}

// Multiple (2) asymmetric CBS servers in isolation
TickType_t cbs_test_9() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(4), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER2_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER2_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID, pdMS_TO_TICKS(40));
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER2_ID, pdMS_TO_TICKS(40));
  return pdMS_TO_TICKS(50);
}

// Multiple (3) asymmetric CBS servers in isolation
TickType_t cbs_test_10() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(2), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER3_ID);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER2_ID, 0);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER3_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER2_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER3_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER1_ID, pdMS_TO_TICKS(40));
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER2_ID, pdMS_TO_TICKS(50));
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER3_ID, pdMS_TO_TICKS(60));
  return pdMS_TO_TICKS(80);
}

// Multiple (2) CBS servers running alongside 1 periodic task
TickType_t cbs_test_11() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(4), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(2), pdMS_TO_TICKS(6), pdMS_TO_TICKS(6), NULL
  );
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER2_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER2_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID, pdMS_TO_TICKS(40));
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER2_ID, pdMS_TO_TICKS(40));
  return pdMS_TO_TICKS(100);
}

// Multiple (3) CBS servers running alongside 1 periodic task
TickType_t cbs_test_12() {
  create_cbs_server(pdMS_TO_TICKS(1), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  create_cbs_server(pdMS_TO_TICKS(2), pdMS_TO_TICKS(8), CBS_SERVER2_ID);
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER3_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(2), pdMS_TO_TICKS(6), pdMS_TO_TICKS(6), NULL
  );
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER2_ID, 0);
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER3_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER2_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER3_ID, pdMS_TO_TICKS(20));
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER1_ID, pdMS_TO_TICKS(40));
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER2_ID, pdMS_TO_TICKS(50));
  CBS_create_aperiodic_task(CBS_task_2, CBS_SERVER3_ID, pdMS_TO_TICKS(60));
  return pdMS_TO_TICKS(100);
}

// 1 CBS Server, 1 periodic task. Bandwidth is high but load of aperiodic tasks is low.
// No deadline miss.
TickType_t cbs_test_13() {
  create_cbs_server(pdMS_TO_TICKS(8), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID, 8);
  CBS_create_aperiodic_task(CBS_task_1, CBS_SERVER1_ID, 16);
  return pdMS_TO_TICKS(21);
}

// 1 CBS Server, 1 periodic task. Bandwidth is high and load of aperiodic tasks is high.
// Deadline miss.
TickType_t cbs_test_14() {
  create_cbs_server(pdMS_TO_TICKS(8), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, 0);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, 1);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, 2);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, 3);
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, 4);
  return pdMS_TO_TICKS(21);
}

// Bandwidth just under deadline miss threshold (Total Utilization < 100%)
TickType_t cbs_test_15() {
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  return pdMS_TO_TICKS(50);
}

// Bandwidth at deadline miss threshold (Total Utilization = 100%)
TickType_t cbs_test_16() {
  create_cbs_server(pdMS_TO_TICKS(3), pdMS_TO_TICKS(7), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  return pdMS_TO_TICKS(50);
}

// Bandwidth just over deadline miss threshold (Total Utilization > 100%)
TickType_t cbs_test_17() {
  create_cbs_server(pdMS_TO_TICKS(4), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  return pdMS_TO_TICKS(50);
}

// 100% server bandwidth
TickType_t cbs_test_18() {
  create_cbs_server(pdMS_TO_TICKS(8), pdMS_TO_TICKS(8), CBS_SERVER1_ID);
  platform_create_periodic_task(
    EDF_periodic_task, "Task P1", pdMS_TO_TICKS(4), pdMS_TO_TICKS(7), pdMS_TO_TICKS(7), NULL
  );
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_4, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  CBS_create_aperiodic_task(CBS_task_3, CBS_SERVER1_ID, pdMS_TO_TICKS(0));
  return pdMS_TO_TICKS(50);
}

#endif // USE_CBS