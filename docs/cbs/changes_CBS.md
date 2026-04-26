# CBS - Changes

## Github Repository

https://github.com/jeromecho/CPSC-538G---FreeRTOS-Projects/tree/4-smp contains the CBS implementation as well as files used for writing RTSIM simulations (refer to `testing_CBS` for more context on RTSIM).

## Kernel files changed
None of the kernel-files were changed in order to support CBS.
## Files changed
We trivially refactored `edf_tests.c`, `srp` `srp_tests.c` to accomodate a new type for FreeRTOS task parameters. Our `ProjectConfig.h` was extended to support automated regression testing for CBS tests (using the testing framework mentioned in our SRP documentation). `edf_scheduler.c` underwent refactoring: 
1) Its `vApplicationTickHook` was extended to support the book-keeping of CBS server budgets and to support the precise release of CBS tasks
2) A new parameter type for FreeRTOS tasks was defined to support the passing of the CBS server metadata to "CBS master tasks" (special aperiodic tasks serving as CBS servers). 

The following configuration constant was introduced to `ProjectConfig.h`

| Constant  | Description                      |
| :-------- | :------------------------------- |
| `USE_CBS` | Flag for toggling CBS on and off |
## Additional files created
`cbs.c` and `cbs.h` were created for the API of creating CBS servers, scheduling aperiodic tasks on particular servers, and for providing APIs that could be plugged-and-played into the `vApplicationTickHook` to support keeping track of the budget of CBS tasks and for releasing soft real-time aperiodic tasks. These files also define constants and data types relevant to CBS logic (e.g., server capacity constants, metadata data types). 

Additionally, the file `test_profiles_cbs.h`  was created. This file held configuration information for the CBS test suite. Notably, the following configuration constant was introduced in `test_profiles_cbs.h`: 

| Constant                   | Description                                                                                                                                                                                           |
| :------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `FAVOUR_SERVER_EQUAL_PRIO` | Flag for toggling between different scheduling policies when two tasks have equal deadline. If enabled, it will always favour scheduling a CBS server if two tasks or servers have the same deadline. |
We introduced this flag as breaking priority ties in favour of the server was a project requirement, however, to enable stress testing with RTSIM (more description in `testing_CBS`), we needed to be able to toggle to a different priority tie breaking policy. 
## Functions introduced

These are public `cbs` functions exposed as API to programs using CBS:

| Function                    | Description                                                                                           |
| :-------------------------- | :---------------------------------------------------------------------------------------------------- |
| `CBS_create_cbs_server`     | Initializes and creates a CBS server (special aperiodic task) assocaited with a particular ID         |
| `CBS_create_aperiodic_task` | Registers a one-time (aperiodic) task (in the form of a function pointer) onto a CBS server of choice |

These are public functions meant to be inserted into the tick hook in a "plug-and-play" fashion to support the logical book-keeping of CBS.

| Function            | Description                                                                                                                                                                               |
| :------------------ | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `CBS_update_budget` | Given the current executing task, updates the budget of the corresponding CBS server if there exists one. Meant to be plugged into the tick hook.                                         |
| `CBS_release_tasks` | Releases any aperiodic tasks into their corresponding CBS servers if the current tick count is greater than or equal to the tasks' release times. Meant to be plugged into the tick hook. |

Additionally, the following private `cbs` functions were implemented.

| Function                       | Description                                                                           |
| :----------------------------- | :------------------------------------------------------------------------------------ |
| `CBS_master_task`              | Task conforming to signature requirements of FreeRTOS tasks. Represents a CBS server. |
| `CBS_master_task_out_of_tasks` | Called by the CBS server to signify it has run out of tasks.                          |

The following functions were introduced to `edf_scheduler`

| Function             | Description                                                                               |
| :------------------- | :---------------------------------------------------------------------------------------- |
| `is_aperiodic_ready` | Used to determine if an aperiodic task (e.g., a CBS master task) is ready to be scheduled |
## Functions changed 
No functions in the FreeRTOS kernel were changed. 
