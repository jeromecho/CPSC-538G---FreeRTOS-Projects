#include "ProjectConfig.h"

#if TEST_SUITE == TEST_SUITE_GLOBAL_MP

#include "global_mp_tests.h"
#include "testing.h"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"

// Consult SMP Documentation for more information on tests
void global_mp_test_1() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 4, 4},
    {EDF_periodic_task, 3, 4, 4}
  };
  build_periodic_test("SMP (Global) Test 1", test_config, MAXIMUM_PERIODIC_TASKS);
}

void global_mp_test_2() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2,  4,  4 },
    {EDF_periodic_task, 10, 20, 20},
    {EDF_periodic_task, 10, 20, 20}
  };
  build_periodic_test("SMP (Global) Test 2", test_config, MAXIMUM_PERIODIC_TASKS);
}

void global_mp_test_3() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 4, 4},
    {EDF_periodic_task, 3, 7, 7},
    {EDF_periodic_task, 3, 5, 5}
  };
  build_periodic_test("SMP (Global) Test 3", test_config, MAXIMUM_PERIODIC_TASKS);
}

#endif // TEST_SUITE == TEST_SUITE_MP
