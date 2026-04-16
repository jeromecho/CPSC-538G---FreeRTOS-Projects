#include "smp_global.h"

#if USE_MP && USE_GLOBAL
#include "admission_control.h"
#include "edf_scheduler.h"
#include "tracer.h"

BaseType_t SMP_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle
) {
#if PERFORM_ADMISSION_CONTROL
  if (!SMP_can_admit_periodic_task(completion_time, period, relative_deadline)) {
    TRACE_record(EVENT_ADMISSION_FAIL(periodic_task_count), TRACE_TASK_PERIODIC, NULL, false);
    TRACE_disable();
    xTaskNotifyGive(monitor_task_handle);
    return pdFALSE;
  }
#endif

  TMB_t     *handle = NULL;
  BaseType_t result = _create_periodic_task_internal(
    task_function,
    task_name,
    periodic_tasks,
    &periodic_task_count,
    // TODO: Decouple SMP private stacks from EDF private stacks (add interface)
    edf_private_stacks_periodic[periodic_task_count],
    completion_time,
    period,
    relative_deadline,
    &handle,
    // TODO: initialize all tasks on core 0 - might have to modify this code so that
    // one iteration of EDF scheduling logic decides cores before the scheduler even
    // starts
    0
  );

  if (result == pdPASS && handle != NULL) {
    if (TMB_handle != NULL) {
      *TMB_handle = handle;
    }
  } else if (result == pdPASS) {
    return pdFAIL;
  }

  return result;
}

#endif // USE_MP && USE_GLOBAL