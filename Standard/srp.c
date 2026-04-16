#include "ProjectConfig.h"

#if USE_SRP

#include "srp.h"

#include "edf_scheduler.h"
#include "scheduler_internal.h"
#include "tracer.h"

#include "config/TestConfig.h"
#if PERFORM_ADMISSION_CONTROL
#include "admission_control.h"
#endif

#include <stdio.h>
#include <string.h> // For memcpy

// Statically allocated state
#if N_RESOURCES == 0
static SRPState_t   srp_state                      = {.priority_ceiling_index = 0, .resource_availability = {}};
static unsigned int resource_ceilings[N_RESOURCES] = {};
#else
static SRPState_t   srp_state = {.priority_ceiling_index = 0, .resource_availability = {[0 ... N_RESOURCES - 1] = 1}};
static unsigned int resource_ceilings[N_RESOURCES] = {0};
#endif // N_RESOURCES

// Statically allocated stack for the stack sharing enabled by SRP
#if ENABLE_STACK_SHARING
StackType_t shared_stacks[N_PREEMPTION_LEVELS][SHARED_STACK_SIZE];
#endif

; // ===================================
; // === LOCAL FUNCTION DECLARATIONS ===
; // ===================================

void srp_specific_initialization(
  TMB_t *task, const unsigned int preemption_level, const TickType_t resource_hold_times[N_RESOURCES]
);


; // ================================
; // === API FUNCTION DEFINITIONS ===
; // ================================

void SRP_push_ceiling(unsigned int new_ceiling) {
  taskENTER_CRITICAL();
  configASSERT(srp_state.priority_ceiling_index < MAX_SRP_NESTING);

  unsigned int current_ceiling   = srp_state.priority_ceiling_stack[srp_state.priority_ceiling_index];
  unsigned int effective_ceiling = MAX(new_ceiling, current_ceiling);

  // Always increment the pointer first. This keeps the first element of the array equal to 0, which is the base
  // (lowest) system ceiling
  srp_state.priority_ceiling_index++;
  srp_state.priority_ceiling_stack[srp_state.priority_ceiling_index] = effective_ceiling;
  taskEXIT_CRITICAL();
}

void SRP_pop_ceiling(void) {
  taskENTER_CRITICAL();
  configASSERT(srp_state.priority_ceiling_index > 0);

  srp_state.priority_ceiling_index--;

  taskEXIT_CRITICAL();
}

// TODO: Semaphore logic should be implemented using actual FreeRTOS semaphores and tick hooks
BaseType_t SRP_take_binary_semaphore(const unsigned int semaphoreIdx) {
  taskENTER_CRITICAL();

  configASSERT(semaphoreIdx < N_RESOURCES);
  configASSERT(srp_state.resource_availability[semaphoreIdx] == 1);

  srp_state.resource_availability[semaphoreIdx] = 0;

  SRP_push_ceiling(resource_ceilings[semaphoreIdx]);

  const TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
  const TMB_t *const current_task        = EDF_get_task_by_handle(current_task_handle);
  TRACE_record(EVENT_SEMAPHORE_TAKE(semaphoreIdx), TRACE_TASK_EITHER, current_task, false);

  scheduler_suspend_and_resume_tasks(0); // Hard-code core 0 here, since SRP should only ever be active on single-core

  taskEXIT_CRITICAL();
  return pdTRUE;
}

void SRP_give_binary_semaphore(const unsigned int semaphoreIdx) {
  taskENTER_CRITICAL();
  configASSERT(semaphoreIdx < N_RESOURCES);

  srp_state.resource_availability[semaphoreIdx] = 1;

  SRP_pop_ceiling();

  const TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
  const TMB_t *const current_task        = EDF_get_task_by_handle(current_task_handle);
  TRACE_record(EVENT_SEMAPHORE_GIVE(semaphoreIdx), TRACE_TASK_EITHER, current_task, false);

  TMB_t *highest_priority_task = scheduler_produce_highest_priority_task();
  if (highest_priority_task != current_task) {
    scheduler_suspend_task(current_task);
  }

  scheduler_suspend_and_resume_tasks(0); // Hard-code core 0 here, since SRP should only ever be active on single-core

  taskEXIT_CRITICAL();
}

/// @brief Getter for the current system ceiling, which is used in the EDF scheduler to determine if
/// a task can preempt or not
unsigned int SRP_get_system_ceiling(void) {
  unsigned int current_ceiling = srp_state.priority_ceiling_stack[srp_state.priority_ceiling_index];
  return current_ceiling;
}

/// @brief Creates a periodic task, making sure all the fields required for SRP are set
BaseType_t SRP_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  const BaseType_t  preemption_level,
  const TickType_t  resource_hold_times[N_RESOURCES]
) {
#if MAXIMUM_PERIODIC_TASKS > 0
  configASSERT(preemption_level <= N_PREEMPTION_LEVELS);
  configASSERT(preemption_level > 0);
  configASSERT(N_PREEMPTION_LEVELS <= MAXIMUM_PERIODIC_TASKS + MAXIMUM_APERIODIC_TASKS);

#if PERFORM_ADMISSION_CONTROL
  if (!SRP_can_admit_periodic_task(completion_time, period, relative_deadline, preemption_level, resource_hold_times)) {
    admission_control_handle_failure(periodic_task_count);
    return pdFALSE;
  }
#endif

#if ENABLE_STACK_SHARING
  StackType_t *stack_buffer = shared_stacks[preemption_level - 1];
#else
  StackType_t *stack_buffer = edf_private_stacks_periodic[periodic_task_count];
#endif

  TMB_t     *handle = NULL;
  BaseType_t result = _create_periodic_task_internal(
    task_function,
    task_name,
    periodic_tasks,
    &periodic_task_count,
    stack_buffer,
    completion_time,
    period,
    relative_deadline,
    &handle,
    configNUMBER_OF_CORES
  );

  if (result == pdPASS) {
    srp_specific_initialization(handle, preemption_level, resource_hold_times);
    if (TMB_handle != NULL) {
      *TMB_handle = handle;
    }
  }

  return result;
#else
  // Fallback for configurations with 0 periodic tasks
  (void)task_function;
  (void)task_name;
  (void)completion_time;
  (void)period;
  (void)relative_deadline;
  (void)TMB_handle;
  (void)preemption_level;
  (void)resource_hold_times;
  return pdFAIL;
#endif
}

/// @brief Creates an aperiodic task, making sure all the fields required for SRP are set
BaseType_t SRP_create_aperiodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  const BaseType_t  preemption_level,
  const TickType_t  resource_hold_times[N_RESOURCES]
) {
#if MAXIMUM_APERIODIC_TASKS > 0
  configASSERT(preemption_level <= N_PREEMPTION_LEVELS);
  configASSERT(preemption_level > 0);
  configASSERT(N_PREEMPTION_LEVELS <= MAXIMUM_PERIODIC_TASKS + MAXIMUM_APERIODIC_TASKS);

#if ENABLE_STACK_SHARING
  StackType_t *stack_buffer = shared_stacks[preemption_level - 1];
#else
  StackType_t *stack_buffer = edf_private_stacks_aperiodic[aperiodic_task_count];
#endif

  TMB_t     *handle = NULL;
  BaseType_t result = _create_aperiodic_task_internal(
    task_function,
    task_name,
    aperiodic_tasks,
    &aperiodic_task_count,
    stack_buffer,
    completion_time,
    release_time,
    relative_deadline,
    &handle,
    NULL,
    true,
    0
  );

  if (result == pdPASS) {
    srp_specific_initialization(handle, preemption_level, resource_hold_times);
    if (TMB_handle != NULL) {
      *TMB_handle = handle;
    }
  }

  return result;
#else
  // Fallback for configurations with 0 aperiodic tasks
  (void)task_function;
  (void)task_name;
  (void)completion_time;
  (void)release_time;
  (void)relative_deadline;
  (void)TMB_handle;
  (void)preemption_level;
  (void)resource_hold_times;
  return pdFAIL;
#endif
}

/// @brief Resets a FreeRTOS task's TCB, so that it can safely use shared stack memory even if it has used it
/// previously. Without this, the task would attempt to pop the registers from the stack in order to resume its previous
/// state, which wouldn't work when the shared stack has been used by another task in the mean time.
void SRP_reset_TCB(const TMB_t *const task) {
  extern StackType_t *pxPortInitialiseStack(StackType_t * pxTopOfStack, TaskFunction_t pxCode, void *pvParameters);

  // Find the top of the stack array
  StackType_t *pxTopOfStack = &(task->stack_buffer[SHARED_STACK_SIZE - 1]);

  // Ensure 8-byte alignment (Required by ARM Cortex-M architecture)
  pxTopOfStack =
    (StackType_t *)(((portPOINTER_SIZE_TYPE)pxTopOfStack) & (~((portPOINTER_SIZE_TYPE)portBYTE_ALIGNMENT_MASK)));

  // Rebuild the ARM hardware stack frame (PC, LR, xPSR, etc.)
  // We pass the completion time back in as the parameter to mimic task creation.
  StackType_t *new_top_of_stack = pxPortInitialiseStack(pxTopOfStack, task->task_function, (void *)&task->parameters);

  // Overwrite the TCB's stack pointer
  *((StackType_t **)task->handle) = new_top_of_stack;
}

; // ==================================
; // === LOCAL FUNCTION DEFINITIONS ===
; // ==================================

/// @brief Get the current resource ceilings in the system
const unsigned int *SRP_get_resource_ceilings() { return resource_ceilings; }

/// @brief Update the resource ceilings in the array passed as the last parameter.
void SRP_update_resource_ceilings(
  const unsigned int preemption_level,
  const TickType_t   resource_hold_times[N_RESOURCES],
  unsigned int       resource_ceilings[N_RESOURCES]
) {
  for (int r = 0; r < N_RESOURCES; r++) {
    if (resource_hold_times[r] > 0 && preemption_level > resource_ceilings[r]) {
      resource_ceilings[r] = preemption_level;
    }
  }
}

/// @brief SRP-specific TMB values which need to be set whenever a task is created
void srp_specific_initialization(
  TMB_t *task, const unsigned int preemption_level, const TickType_t resource_hold_times[N_RESOURCES]
) {
  task->preemption_level = preemption_level;
  task->has_started      = false;
  memcpy(task->resource_hold_times, resource_hold_times, sizeof(task->resource_hold_times));
#if N_RESOURCES > 0
  SRP_update_resource_ceilings(preemption_level, resource_hold_times, resource_ceilings);
#endif
}

#endif // USE_SRP
