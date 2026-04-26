# MP - Future

## Partitioned Scheduling

### Dynamic Load Balancing

Currently, task assignment is static: once a task is pinned to a core at creation time, the only way to move it is via explicit migration calls. A useful improvement would be to automatically monitor per-core utilization and suggest or execute migrations transparently.

This could be done by adding a background monitoring task that periodically checks core utilization and initiates migrations to balance load. This would require a policy for selecting which tasks to migrate and a cost model to avoid unnecessary back-and-forth migrations.

### Intelligent Hyperperiod Alignment on Migration

When a task is migrated to a new core, its release is currently aligned with the destination core's full hyperperiod, which can introduce large delays. A smarter approach would compute the earliest feasible release time that still maintains schedulability on the destination.

### Partitioning Strategy

Currently, tasks are assigned a core explicitly upon creation. This puts the responsibility on the user to balance the load between the tasks and the cores. A better strategy would include a partitioning strategy (like first-fit, best-fit, etc.) that automatically assigns tasks to cores based on their parameters and the current load during creation.

## Global Scheduling

### Improved Testing

For complicated global scheduling stress tests, we used a verifier script to assess the correctness of the traces generated from our tests. While we designed tests for the verifier script and we empirically observed that the verifier was able to pick up on subtle, off-by-one errors in our implementation as we tested our implementation against the verifier, there is no formal "proof" that our verifier is indeed correct in all cases. Exhaustive testing of the verifier script could help build confidence in our verifier. Alternatively, we could also try to resolve the implementation details causing differences in the RTSIM traces and our traces (one specific issue was a race condition between a task being marked done and a task being released, which didn't break correctness, but led to different traces).

### Computational Optimization

Our scheduler implementation currently checks every core and sees if a context switch needs to occur for that core. In certain cases, such as only one task existing, there is no need to see if a context switch needs to occur for the second core if a task has already been scheduled for the first core. In such a context, it makes sense to avoid performing unnecessary computations for the second core. This would not change asymptotic time complexity, assuming the number of cores is fixed, but could provide visible benefits in the microseconds.

## Both Partitioned and Global

### Aperiodic Task Support

Both partitioned and global modes currently focus on periodic tasks. Extending to support aperiodic tasks would increase the expressiveness of the SMP scheduler. Support for aperiodic task currently exists in the code (`SMP_create_aperiodic_task_on_core` in partitioned mode), but isn't tested in any of our tests.

### Support for additional cores

The current implementation assumes a generic two-core SMP system.

To support future platforms with more cores, the implementation should be generalized to handle an arbitrary number of cores (up to `configNUMBER_OF_CORES`). This would involve:

- Generalizing the view sets to support more cores.
- Updating the scheduling logic to consider the top-m tasks across all cores.
- Ensuring that the admission control and migration logic scales appropriately with more cores.
