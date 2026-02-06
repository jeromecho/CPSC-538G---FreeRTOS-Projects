
#ifndef EDF_SCHEDULER_H
#define EDF_SCHEDULER_H

#include "spec.h"

void createTasksFromTestSpec(TestSpec_t *test_spec);

// ASSUMPTION: What the aperiodic task actually does
// is not important. We are simply observing the switching
// in and out of an aperiodic task
