# EDF - bugs

~~There is currently only one bug that we are aware of with our EDF implementation - our admission control is allowing task sets with a total utilization equal to one to be admitted, instead of refusing their admission like we designed it to do. This might be an issue with division involving floating point numbers, which we will address in the future.~~

The above comment was from February, 2026. As of the date of submission, there are no known outstanding bugs for EDF. See code in branch `4-smp` on the repository to see how this issue was resolved.
