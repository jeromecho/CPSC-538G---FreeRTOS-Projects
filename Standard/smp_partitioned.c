#include "smp_partitioned.h"

#include "ProjectConfig.h"

#if USE_MP && USE_PARTITIONED

#include "edf_scheduler.h"

static BaseType_t SMP_pin_to_core(const TMB_t *const task, const UBaseType_t core) {
  if (task == NULL || task->handle == NULL) {
    return pdFAIL;
  }

#if (configUSE_CORE_AFFINITY == 1)
  if (core >= configNUMBER_OF_CORES) {
    return pdFAIL;
  }

  const UBaseType_t core_affinity_mask = ((UBaseType_t)1U) << core;
  vTaskCoreAffinitySet(task->handle, core_affinity_mask);
  return pdPASS;
#else
  (void)core;
  return pdFAIL;
#endif
}

BaseType_t SMP_create_periodic_task_on_core(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  const UBaseType_t core,
  TMB_t **const     TMB_handle
) {
  TMB_t     *task = NULL;
  BaseType_t result =
    EDF_create_periodic_task(task_function, task_name, completion_time, period, relative_deadline, &task);

  if (result != pdPASS || task == NULL) {
    if (TMB_handle != NULL) {
      *TMB_handle = NULL;
    }
    return result;
  }

  result = SMP_pin_to_core(task, core);
  if (result != pdPASS) {
    if (TMB_handle != NULL) {
      *TMB_handle = NULL;
    }
    return pdFAIL;
  }

  if (TMB_handle != NULL) {
    *TMB_handle = task;
  }

  return pdPASS;
}

BaseType_t SMP_create_aperiodic_task_on_core(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  const UBaseType_t core,
  TMB_t **const     TMB_handle
) {
  TMB_t     *task = NULL;
  BaseType_t result =
    EDF_create_aperiodic_task(task_function, task_name, completion_time, release_time, relative_deadline, &task);

  if (result != pdPASS || task == NULL) {
    if (TMB_handle != NULL) {
      *TMB_handle = NULL;
    }
    return result;
  }

  result = SMP_pin_to_core(task, core);
  if (result != pdPASS) {
    if (TMB_handle != NULL) {
      *TMB_handle = NULL;
    }
    return pdFAIL;
  }

  if (TMB_handle != NULL) {
    *TMB_handle = task;
  }

  return pdPASS;
}

#endif
