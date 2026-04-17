#include "ProjectConfig.h"

#if USE_MP
#include "scheduler_internal.h"
#include "smp_shared.h"

SMP_find_task_location(const TaskHandle_t task_handle, SMP_TaskLocation_t *location) {
  if (task_handle == NULL || location == NULL) {
    return false;
  }

  for (UBaseType_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    TMB_t *periodic_match =
      scheduler_search_array_for_handle(task_handle, periodic_tasks[core], periodic_task_count[core]);
    if (periodic_match != NULL) {
      location->task        = periodic_match;
      location->core        = core;
      location->index       = periodic_match->id;
      location->is_periodic = true;
      return true;
    }

    TMB_t *aperiodic_match =
      scheduler_search_array_for_handle(task_handle, aperiodic_tasks[core], aperiodic_task_count[core]);
    if (aperiodic_match != NULL) {
      location->task        = aperiodic_match;
      location->core        = core;
      location->index       = aperiodic_match->id;
      location->is_periodic = false;
      return true;
    }
  }

  return false;
}

void SMP_check_deadlines(void) {
  for (size_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    scheduler_check_deadlines(periodic_tasks[core], periodic_task_count[core]);
    scheduler_check_deadlines(aperiodic_tasks[core], aperiodic_task_count[core]);
  }
}

void SMP_record_releases(void) {
  for (size_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    scheduler_record_releases(periodic_tasks[core], periodic_task_count[core]);
    scheduler_record_releases(aperiodic_tasks[core], aperiodic_task_count[core]);
  }
}

void SMP_suspend_and_resume_tasks(void) {
  for (UBaseType_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    scheduler_suspend_and_resume_tasks(core);
  }
}

void SMP_reschedule_periodic_tasks(void) {
  const TickType_t current_tick = xTaskGetTickCountFromISR();
  for (size_t core = 0; core < configNUMBER_OF_CORES; ++core) {
    for (size_t i = 0; i < periodic_task_count[core]; ++i) {
      TMB_t *const task = &periodic_tasks[core][i];
      (void)scheduler_release_periodic_job_if_ready(task, current_tick);
    }
  }
}

#endif // USE_MP
