# CBS - Future

## Error Handling 

The current type for soft real-time aperiodic tasks (function pointers) scheduled onto a CBS server supports returning values. However, no test cases currently demonstrate the ability to read a value returned by these function pointers. These values could be read by callers, introducing error handling into our CBS implementation. We could achieve this by extending the implementation of the CBS master task, so that it accepts generic error handlers as a function parameter, which can then be used to respond to particular errors. 
## Optimizations

The logic for deciding which tasks to release at a particular tick requires iterating over all pending soft real-time aperiodic tasks and seeing if the current tick is greater than or equal to the task's release time. This incurs $O(n)$ time. By using a min-heap, we can reduce the time complexity of this check to $O(1)$ and also improve the time complexity of adding an aperiodic task to the pool of pending soft real-time aperiodic tasks from $O(n)$ to $O(log(n))$  (currently, we linearly search over a pre-allocated array of slots and look for an empty slot when deciding where to put the metadata for a pending aperiodic task). 
## Logic Extension 

Our CBS implementation is a "soft" implementation - where the deadline of tasks gets postponed if the budget of a CBS server runs out. The RTSIM simulator developed by the ReTIS lab supports "hard" CBS implementations, where tasks get dropped if they miss their deadline. We could easily extend our CBS implementation to toggle between a hard and soft implementation based on a configuration constant. A strategy pattern could be implemented in `cbs` to achieve this. 

