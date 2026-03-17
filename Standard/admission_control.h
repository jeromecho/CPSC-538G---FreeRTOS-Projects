
#include "FreeRTOS.h" // IWYU pragma: keep
#ifndef ADMISSION_CONTROL_H
#define ADMISSION_CONTROL_H

// NB: we seem to treat aperiodic tasks as soft real-time tasks, so current
// admission control implementation only operates over periodic tasks
bool EDF_can_admit_periodic_task( //
  const TickType_t completion_time,
  const TickType_t period,
  const TickType_t relative_deadline
);

#endif // ADMISSION_CONTROL_H
