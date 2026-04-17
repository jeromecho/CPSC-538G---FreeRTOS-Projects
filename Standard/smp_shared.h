#ifndef SMP_SHARED_H
#define SMP_SHARED_H

#if USE_MP

#include "edf_scheduler.h"

typedef struct {
  TMB_t      *task;
  UBaseType_t core;
  size_t      index;
  bool        is_periodic;
} SMP_TaskLocation_t;

bool SMP_find_task_location(const TaskHandle_t task_handle, SMP_TaskLocation_t *location);

void SMP_check_deadlines();
void SMP_record_releases();
void SMP_suspend_and_resume_tasks();
void SMP_reschedule_periodic_tasks();


#endif // USE_MP
#endif // SMP_SHARED_H