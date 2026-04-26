# SRP - Changes

## GIthub Repo

https://github.com/jeromecho/CPSC-538G---FreeRTOS-Projects/tree/2-srp contains the project files at the time this documentation was written.

## Kernel files changed
None of the kernel files were changed in order to support SRP.

## Other files changed
Since the SRP implementation builds on the EDF scheduler, a lot of the changes made to support SRP also affected code written for EDF. EDF functionality ultimately remained the same, but does now have some SRP-logic baked in. Among the files changed were:
`edf_scheduler.c`,
`edf_scheduler.h`,
`admission_control.c`,
`admission_control.h`,
`helpers.c`,
`helpers.h`.

There has also been done a lot of refactoring done in the EDF-specific parts of the project since the EDF documentation was written. This is not included here, as the core functionality of the scheduler remains the same. Function names were changed, and code common to multiple functions was refactored into functions of their own. 

In `FreeRTOSConfig.h`, the flag for enabling static allocation was set to true, and the allocated heap size was reduced to allow the increase in the stack size following the switch from dynamic task creation to static task creation. 

## Additional files created
The files `srp.c` and `srp.h` were created for the SRP API. In addition, the file `ProjectConfig.h` was created to serve as a source for configuration parameters relevant to our scheduler extension, separate from the default FreeRTOS parameters. 

The `ProjectConfig.h` file contains five different constants relevant to SRP:
| Constant | Description |
| :--- | :--- |
| `USE_SRP` | Enables or disables SRP |
| `ENABLE_STACK_SHARING` | Enables or disables stack sharing when using SRP |
| `SHARED_STACK_SIZE` | Determines the size of the stack for all tasks (regardless of whether stack sharing is enabled, contrary to the constant's name) |
| `N_RESOURCES` | The number of resources (binary semaphores) which are going to be used in the program |
| `N_PREEMPTION_LEVELS` | The number of different preemption levels among tasks in the program. These preemption levels can only be 1..N_PREEMPTION_LEVELS |

`srp_tests.c` and `srp_test.h` were also created, and hold all the defined tests we use to determine the correctness of our implementation. 

## Functions introduced
These are public `srp` functions exposed as an API to programs using the scheduler, and to other scheduler extension internals (like `edf_scheduler.c`):
| Function | Description |
| :--- | :--- |
| `SRP_can_admit_periodic_task` | Admission control function specific to tasks created when SRP is enabled |
| `SRP_take_binary_semaphore` | Take a semaphore under the SRP resource policy |
| `SRP_give_binary_semaphore` | Give a semaphore under the SRP resource policy |
| `SRP_get_system_ceiling` | Get the current system ceiling under the SRP resource policy |
| `SRP_push_ceiling` | Add a new element to the ceiling stack under the SRP resource policy |
| `SRP_pop_ceiling` | Remove an element from the ceiling stack under the SRP resource policy |
| `SRP_create_periodic_task` | Creates a periodic task when SRP is enabled. Initializes parameters specific to the SRP implementation, and replaces the standard `EDF_create_periodic_task` |
| `SRP_create_aperiodic_task` | Creates an aperiodic task when SRP is enabled. Initializes parameters specific to the SRP implementation, and replaces the standard `EDF_create_aperiodic_task` |
| `SRP_get_resource_ceilings` | Returns the array of resource ceilings for the system |
| `SRP_update_resource_ceilings` | Updates an array of resource ceilings |
| `SRP_reset_TCB` | Resets a FreeRTOS task's TCB, so that it can safely use shared stack memory even if it has used it previously. Without this, the task would attempt to pop the registers from the stack in order to resume its previous state, which wouldn't work when the shared stack has been used by another task in the mean time. |

In addition, the `srp` module creates some local functions that are not exposed outside of the module:
| Function | Description |
| :--- | :--- |
| `srp_specific_initialization` | Initialization required for tasks which is specific to the SRP implementation. Initializes values in the task's TMB related to a task's preemption level and resource hold times. |

In the `admission_control` module, the following public functions were introduced to deal with SRP-specific implementations:
| Function | Description |
| :--- | :--- |
| `SRP_can_admit_periodic_task` | Checks if a specific task can be admitted into the system. |

In addition, the following functions were defined locally to help calculate admission control criteria:
| Function | Description |
| :--- | :--- |
| `calculate_blocking_time` | Calculates the blocking time B_L defined in section 7.8.4 in the book Hard Real-Time Computing Systems, Fourth Edition. |
| `calculate_B_L` | Calculates the processor demand criterion B(L). Defined in the book Hard Real-Time Computing Systems, Fourth Edition (Eq. 7.24) as the largest amount of time for which a task with relative deadline <= L may be blocked by a task with relative deadline > L. |
| `check_deadlines_srp` | Checks if Eq. 7.25 in Hard Real-Time Computing Systems, Fourth Edition holds when introducing a new task to the system. |

To increase code reuse, a lot of code was abstracted away into helper functions in a `scheduler_internals` file:
| Function | Description |
| :--- | :--- |
| `_create_task_internal` | Helper function for the reusable parts of task creation. |
| `_create_aperiodic_task_internal` | Helper function for the reusable parts of periodic task creation. |
| `_create_periodic_task_internal` | Helper function for the reusable parts of aperiodic task creation. |

This is also where the `TMB_t` type is defined, and where the periodic and aperiodic arrays are made available externally. 

## Functions changed 
We did not change existing functions in the FreeRTOS kernel.
