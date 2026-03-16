#include "srp.h"

#if USE_SRP

#include "edf_scheduler.h"
#include "scheduler_internal.h"
#include "tracer.h"

#include <stdio.h>

// Statically allocated state
static SRPState_t   srp_state;
static unsigned int resource_ceilings[N_RESOURCES];

// Statically allocated stack for the stack sharing enabled by SRP
#if ENABLE_STACK_SHARING
StackType_t shared_stacks[N_PREEMPTION_LEVELS][SHARED_STACK_SIZE];
#endif

// === API FUNCTION DEFINITIONS ===
// ================================

/// @brief Initializes the SRP state for the system. Must be called before any calls to SRP-specific
void SRP_initialize( //
  TMF_t *const              task_matrix,
  const size_t              num_tasks,
  const unsigned int *const user_ceilings_memory
) {
  srp_state.priority_ceiling_index = 0;

  // Initialize all resources to available (1)
  for (int i = 0; i < N_RESOURCES; i++) {
    srp_state.resource_availability[i] = 1;
    resource_ceilings[i]               = 0;
  }

  // Populate the resource ceilings table based on the TMF matrix
  // A resource ceiling is the maximum preemption level of all tasks that use it
  for (size_t t = 0; t < num_tasks; t++) {
    for (int r = 0; r < N_RESOURCES; r++) {
      if (task_matrix[t].resource_hold_times[r] > 0) {
        if (task_matrix[t].preemption_level > resource_ceilings[r]) {
          resource_ceilings[r] = task_matrix[t].preemption_level;
        }
      }
    }
  }

  srp_state.initialized = true;
}

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

BaseType_t SRP_take_binary_semaphore(const unsigned int semaphoreIdx) {
  taskENTER_CRITICAL();
  configASSERT(semaphoreIdx < N_RESOURCES);

  if (srp_state.resource_availability[semaphoreIdx] == 1) {
    srp_state.resource_availability[semaphoreIdx] = 0;

    SRP_push_ceiling(resource_ceilings[semaphoreIdx]);

    const TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
    const TMB_t *const current_task        = EDF_get_task_by_handle(current_task_handle);
    record_trace_event(EVENT_SEMAPHORE_TAKE(semaphoreIdx), TRACE_TASK_EITHER, current_task);

    taskEXIT_CRITICAL();
    return pdTRUE;
  } else {
    taskEXIT_CRITICAL();
    printf(
      "Tick: %u FATAL: SRP Scheduler failed to prevent preemption. Resource %u is locked!\n",
      xTaskGetTickCount(),
      semaphoreIdx
    );

    vTaskSuspendAll(); // Freeze the scheduler to prevent being preempted in the middle of printing the error message
                       // and dumping the trace logs
    print_trace_buffer();
    configASSERT(0);

    return pdFALSE;
  }
}

void SRP_give_binary_semaphore(const unsigned int semaphoreIdx) {
  taskENTER_CRITICAL();
  configASSERT(semaphoreIdx < N_RESOURCES);

  srp_state.resource_availability[semaphoreIdx] = 1;

  SRP_pop_ceiling();

  const TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
  const TMB_t *const current_task        = EDF_get_task_by_handle(current_task_handle);
  record_trace_event(EVENT_SEMAPHORE_GIVE(semaphoreIdx), TRACE_TASK_EITHER, current_task);

  // Figure out the highest priority task from EDF scheduler
  // TaskHandle_t highest_task = produce_highest_priority_task();

  // TODO: Allow preemption immediately after semaphore release?
  // If the highest task's preemption level > SRP_get_system_ceiling():
  //     vTaskResume(highest_task);

  taskEXIT_CRITICAL();
}

/// @brief Getter for the current system ceiling, which is used in the EDF scheduler to determine if
/// a task can preempt or not
unsigned int SRP_get_system_ceiling(void) {
  unsigned int current_ceiling = srp_state.priority_ceiling_stack[srp_state.priority_ceiling_index];
  return current_ceiling;
}

bool SRP_initialized() { return srp_state.initialized; }

/// @brief Creates a periodic task, making sure all the fields required for SRP are set
BaseType_t SRP_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  const BaseType_t  preemption_level
) {
  configASSERT(USE_SRP == 1);
  configASSERT(preemption_level <= N_PREEMPTION_LEVELS);
  configASSERT(preemption_level > 0);

#if ENABLE_STACK_SHARING
  StackType_t *stack_buffer = shared_stacks[preemption_level - 1];
#else
  StackType_t *stack_buffer = edf_private_stacks_periodic[periodic_task_count];
#endif

  TMB_t     *handle = NULL;
  BaseType_t result = _create_periodic_task_internal( //
    task_function,
    task_name,
    stack_buffer,
    completion_time,
    period,
    relative_deadline,
    &handle
  );

  if (result == pdPASS) {
    handle->preemption_level = preemption_level;
    handle->has_started      = false;
    if (TMB_handle != NULL) {
      *TMB_handle = handle;
    }
  }

  return result;
}

/// @brief Creates an aperiodic task, making sure all the fields required for SRP are set
BaseType_t SRP_create_aperiodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  const BaseType_t  preemption_level
) {
  configASSERT(USE_SRP == 1);
  configASSERT(preemption_level <= N_PREEMPTION_LEVELS);
  configASSERT(preemption_level > 0);

#if USE_SRP && ENABLE_STACK_SHARING
  StackType_t *stack_buffer = shared_stacks[preemption_level - 1];
#else
  StackType_t *stack_buffer = edf_private_stacks_aperiodic[aperiodic_task_count];
#endif

  TMB_t     *handle = NULL;
  BaseType_t result = _create_aperiodic_task_internal( //
    task_function,
    task_name,
    stack_buffer,
    completion_time,
    release_time,
    relative_deadline,
    &handle
  );

  if (result == pdPASS) {
    handle->preemption_level = preemption_level;
    handle->has_started      = false;
    if (TMB_handle != NULL) {
      *TMB_handle = handle;
    }
  }

  return result;
}


// === Scheduler internal definitions ===
// ======================================

/// @brief Resets a FreeRTOS task's TCB, so that it can safely use shared stack memory even if it has used it
/// previously. Without this, the task would attempt to pop the registers from the stack in order to resume its previous
/// state, which wouldn't work when the shared stack has been used by another task in the mean time.
void reset_task_stack(const TMB_t *const task) {
  extern StackType_t *pxPortInitialiseStack(StackType_t * pxTopOfStack, TaskFunction_t pxCode, void *pvParameters);

  // Find the top of the stack array
  StackType_t *pxTopOfStack = &(task->stack_buffer[SHARED_STACK_SIZE - 1]);

  // Ensure 8-byte alignment (Required by ARM Cortex-M architecture)
  pxTopOfStack =
    (StackType_t *)(((portPOINTER_SIZE_TYPE)pxTopOfStack) & (~((portPOINTER_SIZE_TYPE)portBYTE_ALIGNMENT_MASK)));

  // Rebuild the ARM hardware stack frame (PC, LR, xPSR, etc.)
  // We pass the completion time back in as the parameter to mimic task creation.
  StackType_t *new_top_of_stack =
    pxPortInitialiseStack(pxTopOfStack, task->task_function, (void *)task->completion_time);

  // Overwrite the TCB's stack pointer
  *((StackType_t **)task->handle) = new_top_of_stack;
}

#endif // USE_SRP
