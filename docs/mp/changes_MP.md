# MP - Changes

## Github Repository

https://github.com/jeromecho/CPSC-538G---FreeRTOS-Projects/tree/4-smp contains SMP implementations as well as verification scripts discussed in `testing_MP`.

## Kernel files changed

None of the FreeRTOS kernel files were changed in order to support SMP. The SMP implementation builds on top of the existing EDF scheduler infrastructure, using FreeRTOS's native SMP support.

## Other files changed

Since SMP builds on top of the EDF implementation, most of the changes to existing files were in the scheduler layer.

The files that were updated are:

| File                   | What changed                                                                                                                                                                                                                                                  |
| :--------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `edf_scheduler.c`      | Added helper functions for multicore scheduling and support for per-core task views in partitioned mode.                                                                                                                                                      |
| `admission_control.c`  | Added `SMP_can_admit_periodic_task()` for global EDF admission control using response-time analysis. Helper functions for the `SMP_can_admit_aperiodic_task()` function were also added. Per-core admission is delegated to the existing EDF admission logic. |
| `admission_control.h`  | Added declarations for SMP-specific admission control.                                                                                                                                                                                                        |
| `scheduler_internal.h` | Extended to expose TMB view management and per-core task state.                                                                                                                                                                                               |

The `ProjectConfig.h` file was extended with the SMP-related constants below:

| Constant          | Description                                                                  |
| :---------------- | :--------------------------------------------------------------------------- |
| `USE_MP`          | Master flag enabling either partitioned or global multiprocessor scheduling. |
| `USE_PARTITIONED` | Enables partitioned EDF scheduling (mutually exclusive with `USE_GLOBAL`).   |
| `USE_GLOBAL`      | Enables global EDF scheduling (mutually exclusive with `USE_PARTITIONED`).   |

## Additional files created

The following files were created specifically for SMP:

| File                             | Purpose                                                                                                     |
| :------------------------------- | :---------------------------------------------------------------------------------------------------------- |
| `smp_partitioned.c`              | Implements partitioned EDF scheduling with per-core task management, admission control, and task migration. |
| `smp_partitioned.h`              | Public API declarations for partitioned scheduling.                                                         |
| `smp_global.c`                   | Implements global EDF scheduling with a single ready queue.                                                 |
| `smp_global.h`                   | Public API declarations for global scheduling.                                                              |
| `partitioned_mp_tests.c`         | Contains the 19 partitioned scheduling test cases.                                                          |
| `partitioned_mp_tests.h`         | Declares the partitioned test entry points.                                                                 |
| `global_mp_tests.c`              | Contains the 11 global scheduling test cases.                                                               |
| `global_mp_tests.h`              | Declares the global test entry points.                                                                      |
| `test_profiles_partitioned_mp.h` | Declares the partitioned test configuration constants.                                                      |
| `test_profiles_global_mp.h`      | Declares the global test configuration constants.                                                           |

## Functions introduced

### Partitioned Scheduling API

The below is the public API of `smp_partitioned`:

| Function                                        | Description                                                                                         |
| :---------------------------------------------- | :-------------------------------------------------------------------------------------------------- |
| `SMP_create_periodic_task_on_core`              | Creates a periodic task pinned to a specific core. Runs per-core EDF admission control.             |
| `SMP_create_aperiodic_task_on_core`             | Creates an aperiodic task pinned to a specific core.                                                |
| `SMP_remove_task_from_core`                     | Removes a task from a specific core and deletes its FreeRTOS TCB.                                   |
| `SMP_migrate_task_to_core`                      | Migrates a currently suspended task to a different core, with admission control on the destination. |
| `SMP_partitioned_produce_highest_priority_task` | Returns the task with the earliest deadline on a specific core.                                     |
| `SMP_partitioned_suspend_lower_priority_tasks`  | Suspends all lower-priority tasks on a core except the highest-priority one.                        |
| `SMP_partitioned_reschedule_periodic_tasks`     | Reschedules periodic tasks on all cores.                                                            |
| `SMP_partitioned_check_deadlines`               | Checks for deadline misses on a specific core.                                                      |
| `SMP_partitioned_record_releases`               | Records the release of periodic tasks on a specific core.                                           |

The following local helpers support partitioned scheduling:

| Function                                                   | Description                                                                    |
| :--------------------------------------------------------- | :----------------------------------------------------------------------------- |
| `SMP_view_add`                                             | Adds a task to a per-core view set.                                            |
| `SMP_view_remove`                                          | Removes a task from a per-core view set.                                       |
| `SMP_get_view_set`                                         | Retrieves the view set for a specific core (periodic or aperiodic).            |
| `SMP_find_task_in_view_by_handle`                          | Searches for a task in a view by its FreeRTOS handle.                          |
| `SMP_find_task_location`                                   | Finds which core a task is assigned to.                                        |
| `SMP_can_admit_periodic_task_on_core`                      | Runs EDF admission control for a single core.                                  |
| `SMP_can_admit_migrated_periodic_task_on_core`             | Checks if a task can be admitted on a destination core after migration.        |
| `SMP_calculate_aligned_release_for_migrated_periodic_task` | Aligns a migrated task's release time with the destination core's hyperperiod. |

### Global Scheduling API

The below is the public API of `smp_global`:

| Function                                    | Description                                                                        |
| :------------------------------------------ | :--------------------------------------------------------------------------------- |
| `SMP_create_periodic_task`                  | Creates a periodic task for global scheduling.                                     |
| `SMP_global_produce_highest_priority_tasks` | Produces 2 highest priority tasks by running search without replacement twice.     |
| `SMP_global_check_deadlines`                | Checks for deadline misses of tasks.                                               |
| `SMP_global_record_releases`                | Records releases for tasks using tracer.                                           |
| `SMP_suspend_and_resume_tasks`              | Suspends all tasks save two highest priority tasks. Meant to be used in tick hook. |
| `SMP_global_reschedule_periodic_tasks`      | Reschedules periodic tasks that have finished and are release once more.           |
| `SMP_global_reschedule_periodic_tasks`      | Reschedules periodic tasks that have finished and are release once more.           |

The following shows additions to `admission_control`'s public API:

| Function                      | Description                                                                        |
| :---------------------------- | :--------------------------------------------------------------------------------- |
| `SMP_can_admit_periodic_task` | Performs admission control using Bertogna and Cirinei's (2007) RTA for global EDF. |

The following is `admission_control`'s helpers:

| Function                                   | Description                                                        |
| :----------------------------------------- | :----------------------------------------------------------------- |
| `response_time_analysis_for_task_in_array` | Performs RTA for a task in global EDF.                             |
| `bounded_interference`                     | Produces maximum interference for a particular task in global EDF. |
| `worload_in_interval`                      | Calculates maximum workload of a task in an interval.              |

The following local helpers support global scheduling
| Function | Description |
| :----------------------------------------------- | :----------------------------------------------------------------------------------------------------------- |
| `multicore_stability_comparator` | Comparator used for deciding the highest priority task. |
| `SMP_highest_priority_task_multicore` | Produces highest priority task. |
| `SMP_global_should_context_switch` | Produce true if scheduler should context switch on a particular core. Produce false otherwise. |
| `SMP_global_mark_highest_priority_tasks` | Marks a task to be scheduled and not among the unscheduled highest priority tasks. |
| `SMP_global_get_target_task` | Produce highest priority task to schedule on core. |
| `SMP_global_global_suspend_lower_priority_tasks` | Suspends all tasks that aren't one of the 2 highest priority tasks. |
| `_len_highest_priority_tasks` | Produce number of highest priority tasks (produce up to `m` tasks, where `m` is number of processors). |

## Functions changed

We did not change existing functions in the FreeRTOS kernel.

The existing EDF scheduler functions were extended to support per-view admission control in partitioned mode. In global mode, the global EDF ready queue and priority assignment logic are used as-is, with SMP-aware context switching logic layered on top.
