/**
 * Central location to define all tasks
 */

#include "FreeRTOS.h"
#include "tests.h"

// periodic, pre-emptive, deadline == period, same release times
static const TaskSpec_t test1_tasks[] = {
  {"P_T1", GPIO_PIN_2, pdMS_TO_TICKS(200), pdMS_TO_TICKS(600), pdMS_TO_TICKS(600), 0, 1, PERIODIC},
  {"P_T2", GPIO_PIN_2, pdMS_TO_TICKS(100), pdMS_TO_TICKS(200), pdMS_TO_TICKS(200), 0, 2, PERIODIC},
};

extern const TestSpec tests[] = {
  {test1_tasks, 2}
};

extern const test_count = 1;
