# EDF - Changes
## Github Repo

https://github.com/jeromecho/CPSC-538G---FreeRTOS-Projects/tree/1-edf showcases the state of the codebase at the time of this documentation.

## Kernel files changed
None of the kernel-files were changed in order to support EDF.

## Other files changed
In `FreeRTOSConfig.h`, the function declaration for tick hook functions were added. These are necessary in order for the kernel code to have access to the function prototypes during compilation, since the functions are defined outside the kernel itself. In addition, the priority of the timer task was lowered to 0 to prevent it from interfering with the execution of periodic tasks. 

## Additional files created
The files `edf_scheduler.c` and `edf_scheduler.h` were created for the scheduling API, and `admission_control.c` and `admission_control.h` implement functions for admission control. In addition, the files `helpers.c` and `helper.h` were added to implement common utility functions like `gcd` and `lcm`. Together, these files hold all the relevant pieces for our abstraction layer built on top of the FreeRTOS scheduler. 

## Functions introduced
These are public `edf_scheduler` functions exposed as API to programs using the scheduler:

| Function | Description |
| :--- | :--- |
| `xTaskCreatePeriodic` | Initializes and registers a new periodic task |
| `xTaskCreateAperiodic` | Registers a one-time (aperiodic) task into the scheduler. **Note:** while implemented, this function is untested due to not being a requirement for this part of the project. Additionaly work on this function will follow in the CBS part of the project. |
| `taskDone` | Called by the client at the end of a task loop to signal completion for periodic tasks. |

Additionally, the following private `edf_scheduler` functions were implemented.
| Function | Description |
| :--- | :--- |
| `setSchedulable` | Move periodic tasks that have finished into "ready" state after release time arrives |
| `produce_highest_priority_task` | Identifies the task with the earliest deadline among ready tasks |
| `setHighestPriority` | Assigns the top priority level to inputted task. |
| `deprioritizeAllTasks` | Lowers the priority of all tasks. Called when changing the assignment of the highest priority task. |
| `resumeAllTasks` | Restores tasks to their ready state after a scheduler suspension. |
| `calculate_release_time_for_dropped_task` | Determines the next valid start time for a task that was added while the system was running - synchronizing added task with existing task set. |
| `should_update_priorities` | Logic check to determine if a preemption is required by checking if currently running task equals the highest priority task. |
| `updatePriorities` | Update priorities of task so that task with earliest deadline has highest priority. |
| `vApplicationTickHook` | RTOS hook that executes every system tick to update the scheduler's state (more description in "Design" section). |

Finally, the following two functions are used for debugging by setting GPIO pins high and low when tasks are switched in or out:
| Function | Description |
| :--- | :--- |
| `task_switched_out` | Sets a designated GPIO pin **Low** when a task stops executing. |
| `task_switched_in` | Sets a designated GPIO pin **High** when a task begins execution. |

In the `helpers` files, the functions defined are:
| Function | Description |
| :--- | :--- |
| `gcd` | Calculates the Greatest Common Divisor for hyperperiod logic. |
| `lcm` | Calculates the Least Common Multiple for hyperperiod logic. |
| `compute_hyperperiod` | Determines the hyperperiod of the periodic task set. |

In the `admission_control` files, the functions defined are:
| Function | Description |
| :--- | :--- |
| `can_admit_periodic_task` | Primary check to determine if a new task can be added without missing deadlines. Implements procedure demand based admission control (more details in "Design" section). |

This function relies on a number of private functions:
| Function | Description |
| :--- | :--- |
| `dbf` | **Demand Bound Function**: Calculates the maximum processor demand in a synchronized time interval starting from time point 0 up to some given time point L. |
| `calculate_l_star` | Computes the upper bound for the feasibility check duration. Used to optimize admission control by reducing number of checks. |
| `calculate_d_max` | Finds the maximum deadline among all tasks to bound the feasibility calculation. |
| `check_deadlines` | Checks that demand bound function evaluates to less than or equal to available time at every absolute deadline that needs to be checked in procedure demand based admission control (see "Design" section for details) |

## Functions changed 
We did not change existing functions. 
