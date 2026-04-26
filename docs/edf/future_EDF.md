# EDF - Future

## Design changes
In the future, we might want to move from an extension designed as an abstraction layer on top of FreeRTOS to modifying the kernel code itself, especially if this was to be merged into the official project. This would help make the project more easily maintained and integrated into the existing FreeRTOS codebase; creating an abstraction layer and tick hook every time we want to extend FreeRTOS for a new algorithm could become messy very quickly. Furthermore, moving logic to the kernel could offer performance benefits. 

## Optimizations
There are some obvious optimizations we could make to the way our scheduler extension's logic is executed. Currently, every time our tick hook function is called, we are making several O(n) passes over a list of periodic tasks. This is done in order to determine which task has the earliest deadline, which tasks need to have their priority reduced, and which tasks need to be resumed from a suspended state. When the number of tasks increases, this overhead might become detrimental to the performance of the actual application being run by the scheduler. To mitigate this, we could move away from our current method of iteration every time slice, in favor of a more event-driven solution; our scheduler extension should only ever need to make any changes after specific events (e.g., task creation, completion, and release). 

By setting a flag to indicate when the scheduler needs to reasess/recalculate the next earliest deadline, we remove the need to check if the earliest deadline among tasks has changed, allowing for a simple O(1) check every tick hook. In addition, a simple optimization to be implemented would to collapse the multiple iterations through the same array of TMB's into a single iteration, preventing a lot of the overhead with our current solution without actually changing its design. 

An alternative to the approach mentioned above would be to maintain some sort of priority queue for all periodic tasks, ordered by their deadlines. This would allow for maximally efficient operations; checking if the task with the earliest deadline has changed would be an O(1) operation, while insertion and removal of jobs from the list would change to an O(log(n)) operation, compared to our current O(n) approach. 

## API simplifications
With our current implementation, tasks need to mark themselves as complete to inform the scheduler that they do not need any more execution time before their deadline/next period, and then need to suspend themselves. However, we would like to simplify the API such that a task can call a function to suspend itself only until its next period starts, at which point the scheduler should "wake" it again. This would clarify the intended usage of each function and limit the points of interaction between the application and the kernel.

