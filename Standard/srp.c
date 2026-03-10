#include "srp.h"

#if USE_SRP

#include "edf_scheduler.h" // Needed to find the highest priority task on give
#include "tracer.h"

#include <stdio.h>

// Statically allocated state
static SRPState_t   srp_state;
static unsigned int resource_ceilings[N_RESOURCES];

// Statically allocated stack for O(1) state restoration
// Max depth is bounded by N_RESOURCES because a task can only take each resource once
static SRP_Stack_Element_t srp_stack[N_RESOURCES];
static int                 srp_stack_pointer = -1;

// === API FUNCTION DEFINITIONS ===

/// @brief Initializes the SRP state for the system. Must be called before any calls to SRP-specific
void SRP_initialize( //
  TMF_t *const              task_matrix,
  const size_t              num_tasks,
  const unsigned int *const user_ceilings_memory
) {
  srp_state.global_priority_ceiling = 0; // Assuming 0 is the lowest preemption level

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

BaseType_t SRP_take_binary_semaphore(const unsigned int semaphoreIdx) {
  taskENTER_CRITICAL();

  configASSERT(semaphoreIdx < N_RESOURCES);

  if (srp_state.resource_availability[semaphoreIdx] == 1) {
    // Resource is available. Update availability.
    srp_state.resource_availability[semaphoreIdx] = 0;

    // This is only for tracing/debugging purposes to show which task took which resource at what
    // time. It is not needed for the actual SRP logic.
    const TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
    const TMB_t *const current_task        = EDF_get_task_by_handle(current_task_handle);
    configASSERT(current_task != NULL);

    // Push current ceiling and semaphoreIdx onto the stack
    srp_stack_pointer++;
    srp_stack[srp_stack_pointer].previous_global_ceiling = srp_state.global_priority_ceiling;
    srp_stack[srp_stack_pointer].semaphore_idx           = semaphoreIdx;

    // Update global priority ceiling
    unsigned int resource_ceiling = resource_ceilings[semaphoreIdx];
    if (resource_ceiling > srp_state.global_priority_ceiling) {
      srp_state.global_priority_ceiling = resource_ceiling;
    }
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

// TODO: Create push() and pop() function for SRP stack, which should return an error if the srp_stack_pointer exceeds
// the upper/lower bounds
void SRP_give_binary_semaphore(const unsigned int semaphoreIdx) {
  configASSERT(semaphoreIdx < N_RESOURCES);
  configASSERT(srp_stack_pointer >= 0);

  taskENTER_CRITICAL();

  // Pop the state
  SRP_Stack_Element_t popped_state = srp_stack[srp_stack_pointer];
  srp_stack_pointer--;

  // Restore the old global ceiling and set resource to available
  srp_state.global_priority_ceiling                           = popped_state.previous_global_ceiling;
  srp_state.resource_availability[popped_state.semaphore_idx] = 1;

  // This is only for tracing/debugging purposes to show which task took which resource at what
  // time. It is not needed for the actual SRP logic.
  const TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
  const TMB_t *const current_task        = EDF_get_task_by_handle(current_task_handle);
  configASSERT(current_task != NULL);
  record_trace_event(EVENT_SEMAPHORE_GIVE(semaphoreIdx), TRACE_TASK_EITHER, current_task);

  // Figure out the highest priority task from EDF scheduler
  // TaskHandle_t highest_task = produce_highest_priority_task();

  // TODO: Look up the preemption level of 'highest_task'?
  // If the highest task's preemption level > srp_state.global_priority_ceiling:
  //     vTaskResume(highest_task);

  taskEXIT_CRITICAL();
}

/// @brief Getter for the current system ceiling, which is used in the EDF scheduler to determine if
/// a task can preempt or not
unsigned int SRP_get_system_ceiling(void) { return srp_state.global_priority_ceiling; }

bool SRP_initialized() { return srp_state.initialized; }

#endif // USE_SRP
