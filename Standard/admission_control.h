
#ifndef ADMISSION_CONTROL_H
#define ADMISSION_CONTROL_H

#include "ProjectConfig.h"

#if USE_EDF

#include "FreeRTOS_include.h" // IWYU pragma: keep
#include "types/scheduler_types.h"

// NB: we seem to treat aperiodic tasks as soft real-time tasks, so current
// admission control implementation only operates over periodic tasks
bool EDF_can_admit_periodic_task_for_task_set(
  const TickType_t completion_time,
  const TickType_t period,
  const TickType_t relative_deadline,
  const TMB_t     *tasks,
  const size_t     task_count
);

void admission_control_handle_failure(const uint32_t uid);

#if !(USE_MP && USE_PARTITIONED)
bool EDF_can_admit_periodic_task( //
  const TickType_t completion_time,
  const TickType_t period,
  const TickType_t relative_deadline
);
#endif

bool SRP_can_admit_periodic_task(
  const TickType_t   completion_time,
  const TickType_t   period,
  const TickType_t   relative_deadline,
  const unsigned int preemption_level,
  const TickType_t  *resource_hold_times
);

#endif // USE_EDF

#endif // ADMISSION_CONTROL_H
