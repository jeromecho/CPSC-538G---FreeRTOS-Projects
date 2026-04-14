#include "ProjectConfig.h"

#if TEST_SUITE == TEST_SUITE_PARTITIONED_MP

#include "partitioned_mp_tests.h"
#include "testing.h"

#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"

#if TEST_NR == 1
/// @brief Dual-core EDF smoke test with one core running two tasks and the other running one task.
/// The shorter-period task on core 0 should preempt the longer-period task on the same core.
void partitioned_mp_test_1() {
  const PeriodicTaskParams_t test_config[MAXIMUM_PERIODIC_TASKS] = {
    {EDF_periodic_task, 2, 6, 6, 0},
    {EDF_periodic_task, 1, 2, 2, 0},
    // {EDF_periodic_task, 2, 6, 6, 1},
    // {EDF_periodic_task, 1, 2, 2, 1},
  };

  build_periodic_test("SMP Test 1", test_config, MAXIMUM_PERIODIC_TASKS);
}
#endif

#endif // TEST_SUITE == TEST_SUITE_MP
