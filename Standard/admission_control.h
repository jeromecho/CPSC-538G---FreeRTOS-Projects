
#include "FreeRTOS.h"
#ifndef ADMISSION_CONTROL_H
#define ADMISSION_CONTROL_H

// NB: we seem to treat aperiodic tasks as soft real-time tasks, so current
// admission control implementation only operates over periodic tasks
bool can_admit_periodic_task(
  TickType_t completion_time, TickType_t period, TickType_t relative_deadline
);
#endif
