#include "testing.h"

#include "edf_scheduler.h"
#include "helpers.h"

#include <stdio.h>

#if TEST_SUITE == TEST_SUITE_SRP
#include "srp.h"
#elif TEST_SUITE == TEST_SUITE_PARTITIONED_MP
#include "smp_partitioned.h"
#elif TEST_SUITE == TEST_SUITE_GLOBAL_MP
#include "smp_global.h"
#endif

/// @brief Creates a periodic task from a provided task configuration.
// Handles API differences between SRP and EDF via preprocessor macros.
void build_periodic_task(const char *task_name, const PeriodicTaskParams_t *config) {
#if TEST_SUITE == TEST_SUITE_EDF
  EDF_create_periodic_task( //
    config->func,
    task_name,
    pdMS_TO_TICKS(config->C),
    pdMS_TO_TICKS(config->T),
    pdMS_TO_TICKS(config->D),
    NULL
  );
#elif TEST_SUITE == TEST_SUITE_SRP
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
#elif TEST_SUITE == TEST_SUITE_CBS
  (void)0;
#elif TEST_SUITE == TEST_SUITE_PARTITIONED_MP
  SMP_create_periodic_task_on_core( //
    config->func,
    task_name,
    pdMS_TO_TICKS(config->C),
    pdMS_TO_TICKS(config->T),
    pdMS_TO_TICKS(config->D),
    config->core,
    NULL
  );
#elif TEST_SUITE == TEST_SUITE_GLOBAL_MP
#error "Global partitioning not implemented yet"

#else
#error "Scheduler type not defined! Define USE_SRP or USE_EDF."
#endif
}

/// @brief Creates an aperiodic task from a provided task configuration
void build_aperiodic_task(const char *task_name, const AperiodicTaskParams_t *config) {
#if TEST_SUITE == TEST_SUITE_EDF
  EDF_create_aperiodic_task( //
    config->func,
    task_name,
    pdMS_TO_TICKS(config->C),
    pdMS_TO_TICKS(config->r),
    pdMS_TO_TICKS(config->D),
    NULL,
    NULL,
    true
  );
#elif TEST_SUITE == TEST_SUITE_SRP
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
#elif TEST_SUITE == TEST_SUITE_CBS
  (void)0;
#elif TEST_SUITE == TEST_SUITE_PARTITIONED_MP
  SMP_create_aperiodic_task_on_core(
    config->func,
    task_name,
    pdMS_TO_TICKS(config->C),
    pdMS_TO_TICKS(config->r),
    pdMS_TO_TICKS(config->D),
    config->core,
    NULL
  );
#elif TEST_SUITE == TEST_SUITE_GLOBAL_MP
#error "Global partitioning not implemented yet"

#else
#error "Scheduler type not defined! Define TEST_SUITE_SRP, TEST_SUITE_EDF, or TEST_SUITE_MP."
#endif
}

/// @brief Creates all tasks from the provided test configuration for periodic tasks
void build_periodic_test( //
  const char                 *test_name,
  const PeriodicTaskParams_t *config,
  size_t                      num_tasks
) {
  configASSERT(num_tasks == (MAXIMUM_PERIODIC_TASKS + MAXIMUM_APERIODIC_TASKS));

  for (size_t i = 0; i < num_tasks; i++) {
    char task_name[22]; // Exactly enough for "SRP Test XX, Task YYY", plus a null terminator byte
    snprintf(task_name, sizeof(task_name), "%s, Task %d", test_name, (int)(i + 1));
    build_periodic_task(task_name, &config[i]);
  }
}

/// @brief Creates all tasks from the provided test configuration for aperiodic tasks
void build_aperiodic_test( //
  const char                  *test_name,
  const AperiodicTaskParams_t *config,
  size_t                       num_tasks
) {
  configASSERT(num_tasks == (MAXIMUM_PERIODIC_TASKS + MAXIMUM_APERIODIC_TASKS));

  for (size_t i = 0; i < num_tasks; i++) {
    char task_name[22]; // Exactly enough for "SRP Test XX, Task YYY", plus a null terminator byte
    snprintf(task_name, sizeof(task_name), "%s, Task %d", test_name, (int)(i + 1));
    build_aperiodic_task(task_name, &config[i]);
  }
}

/// @brief Execute for a series of ticks without marking as done
void execute_for_ticks(const TickType_t execution_ticks) {
  TickType_t time_executed = 0;
  TickType_t previous_tick = -1;
  while (time_executed < execution_ticks) {
    const TickType_t current_tick = xTaskGetTickCount();
    if (current_tick != previous_tick) {
      time_executed += 1;
      previous_tick = current_tick;
    }
  }
}

/// @brief Executes a series of steps defined for a given test. Verifies that the total execution time for the task
/// matches the intended completion time
void task_execute(const TaskWorkload_t *task_workload, const size_t num_steps) {
  // Ensure that none of the actions are scheduled to happen:
  // - Before the first tick of the task's execution
  // - After completion time
  for (size_t i = 0; i < num_steps; i++) {
    const TaskStep_t *action = &task_workload->task_actions[i];
    configASSERT(action->relative_tick > 0);
    configASSERT(action->relative_tick <= task_workload->completion_time);
  }

  size_t     action_index  = 0;
  TickType_t time_executed = 0;
  TickType_t previous_tick = -1;
  while (time_executed < task_workload->completion_time) {
    const TickType_t current_tick = xTaskGetTickCount();
    if (current_tick != previous_tick) {
      time_executed += 1;
      previous_tick = current_tick;

      if (action_index >= num_steps) {
        continue;
      }

      const TaskStep_t *next_step = &task_workload->task_actions[action_index];
      if (next_step->relative_tick == time_executed) {
        action_index += 1;
        switch (next_step->action) {
#if TEST_SUITE == TEST_SUITE_SRP
        case TASK_TAKE_SEMAPHORE:
          SRP_take_binary_semaphore(next_step->semaphore_index);
          break;

        case TASK_GIVE_SEMAPHORE:
          SRP_give_binary_semaphore(next_step->semaphore_index);
          break;
#endif

        default:
          break;
        }
      }
    }
  }

  // Mark as done
  EDF_mark_task_done(NULL);
}
