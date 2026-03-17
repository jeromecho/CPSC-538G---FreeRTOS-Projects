#include "srp.h"

#if USE_SRP

#include "edf_scheduler.h"
#include "helpers.h"
#include "scheduler_internal.h"
#include "tracer.h"

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

void update_resource_ceilings(
  const unsigned int preemption_level,
  const TickType_t   resource_hold_times[N_RESOURCES],
  unsigned int      *resource_ceilings
);
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

  if (srp_state.resource_availability[semaphoreIdx] == 1) {
    srp_state.resource_availability[semaphoreIdx] = 0;

    SRP_push_ceiling(resource_ceilings[semaphoreIdx]);

    const TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
    const TMB_t *const current_task        = EDF_get_task_by_handle(current_task_handle);
    TRACE_record(EVENT_SEMAPHORE_TAKE(semaphoreIdx), TRACE_TASK_EITHER, current_task);

    taskEXIT_CRITICAL();
    return pdTRUE;
  } else {
    taskEXIT_CRITICAL();
    crash_with_trace(
      "Tick: %u FATAL: SRP Scheduler failed to prevent preemption. Resource %u is locked!\n",
      xTaskGetTickCount(),
      semaphoreIdx
    );
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
  TRACE_record(EVENT_SEMAPHORE_GIVE(semaphoreIdx), TRACE_TASK_EITHER, current_task);

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
  configASSERT(USE_SRP == 1);
  configASSERT(preemption_level <= N_PREEMPTION_LEVELS);
  configASSERT(preemption_level > 0);

#if PERFORM_ADMISSION_CONTROL
  // TODO: Should admission control be extended to aperiodic tasks?
  if (!SRP_can_admit_periodic_task(completion_time, period, relative_deadline, preemption_level, resource_hold_times)) {
    crash_without_trace("%s - Admission failed for: %s\n", __func__, task_name);
  }
#endif // PERFORM_ADMISSION_CONTROL

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
    srp_specific_initialization(handle, preemption_level, resource_hold_times);
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
  const BaseType_t  preemption_level,
  const TickType_t  resource_hold_times[N_RESOURCES]
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
    srp_specific_initialization(handle, preemption_level, resource_hold_times);
    if (TMB_handle != NULL) {
      *TMB_handle = handle;
    }
  }

  return result;
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
  StackType_t *new_top_of_stack =
    pxPortInitialiseStack(pxTopOfStack, task->task_function, (void *)task->completion_time);

  // Overwrite the TCB's stack pointer
  *((StackType_t **)task->handle) = new_top_of_stack;
}

/// @brief Calculates the maximum blocking time B_k for a task at a given preemption level.
static TickType_t calculate_blocking_time(
  const unsigned int  target_preemption_level,
  const unsigned int *simulated_ceilings,
  const unsigned int  new_task_preemption_level,
  const TickType_t   *new_task_resource_holds
) {
  TickType_t max_blocking = 0;

  // Check existing tasks to see if they can block a task at the target level
  for (size_t i = 0; i < periodic_task_count; i++) {
    // A task can only block us if it has a STRICTLY LOWER preemption level
    if (periodic_tasks[i].preemption_level < target_preemption_level) {
      for (int r = 0; r < N_RESOURCES; r++) {
        // If the lower-level task uses this resource, AND the resource's ceiling
        // is high enough to block our target level...
        if (periodic_tasks[i].resource_hold_times[r] > 0 && simulated_ceilings[r] >= target_preemption_level) {
          max_blocking = MAX(max_blocking, periodic_tasks[i].resource_hold_times[r]);
        }
      }
    }
  }

  // Check the new task (since it isn't in the periodic_tasks array yet)
  if (new_task_preemption_level < target_preemption_level) {
    for (int r = 0; r < N_RESOURCES; r++) {
      if (new_task_resource_holds[r] > 0 && simulated_ceilings[r] >= target_preemption_level) {
        max_blocking = MAX(max_blocking, new_task_resource_holds[r]);
      }
    }
  }

  return max_blocking;
}

/// @brief Checks if it is possible to admit another task with the provided parameters to the system.
bool SRP_can_admit_periodic_task( //
  const TickType_t   completion_time,
  const TickType_t   period,
  const TickType_t   relative_deadline,
  const unsigned int preemption_level,
  const TickType_t   resource_hold_times[N_RESOURCES]
) {
  // printf("Admission check when creating task with C=%d\n", completion_time);
  // Create an array of simulated ceilings for the admission test to run on
  unsigned int simulated_ceilings[N_RESOURCES];
  memcpy(simulated_ceilings, resource_ceilings, sizeof(simulated_ceilings));
  update_resource_ceilings(preemption_level, resource_hold_times, simulated_ceilings);

  // Utilization. Checks if the existing tasks survive the introduction of the new task
  for (size_t k = 0; k < periodic_task_count; k++) {
    const TickType_t   D_k                = periodic_tasks[k].periodic.relative_deadline;
    const TickType_t   T_k                = periodic_tasks[k].periodic.period;
    const unsigned int preemption_level_k = periodic_tasks[k].preemption_level;

    double sum_U = 0.0;

    // Sum utilization of all existing tasks with D_i <= D_k
    for (size_t i = 0; i < periodic_task_count; i++) {
      if (periodic_tasks[i].periodic.relative_deadline <= D_k) {
        sum_U += (double)periodic_tasks[i].completion_time / periodic_tasks[i].periodic.period;
      }
    }

    // Include the new task's utilization if its deadline is <= D_k
    if (relative_deadline <= D_k) {
      sum_U += (double)completion_time / period;
    }

    // Calculate maximum blocking time for task k using the simulated ceilings
    const TickType_t B_k = calculate_blocking_time( //
      preemption_level_k,
      simulated_ceilings,
      preemption_level,
      resource_hold_times
    );

    // Evaluate Baker's Condition for task k
    const double condition = sum_U + ((double)B_k / T_k);
    // printf(
    //   "Task %d (C=%d) - U: %f, B: %d, Cond: %f\n", k + 1, periodic_tasks[k].completion_time, sum_U, B_k, condition
    // );
    if (condition > 1.0) {
      return false; // Admission failed: Task k is not schedulable
    }
  }

  // Check if the new task can run alongside the existing tasks
  // Sum utilization of all existing tasks with D_i <= D_new
  double sum_U_new = 0.0;
  for (size_t i = 0; i < periodic_task_count; i++) {
    if (periodic_tasks[i].periodic.relative_deadline <= relative_deadline) {
      sum_U_new += (double)periodic_tasks[i].completion_time / periodic_tasks[i].periodic.period;
    }
  }
  sum_U_new += (double)completion_time / period; // Add the new task's own utilization

  // Calculate blocking time for the new task
  const TickType_t B_new = calculate_blocking_time( //
    preemption_level,
    simulated_ceilings,
    preemption_level,
    resource_hold_times
  );

  // Evaluate Baker's Condition for the new task
  const double condition = sum_U_new + ((double)B_new / period);
  // printf("New Task (C=%d) - U: %f, B: %d, Cond: %f\n", completion_time, sum_U_new, B_new, condition);
  if (condition > 1.0) {
    return false; // Admission failed: The new task is not schedulable
  }

  return true;
}

; // ==================================
; // === LOCAL FUNCTION DEFINITIONS ===
; // ==================================

/// @brief Update the resource ceilings in the array passed as the last parameter.
void update_resource_ceilings(
  const unsigned int preemption_level,
  const TickType_t   resource_hold_times[N_RESOURCES],
  unsigned int      *resource_ceilings
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
  memcpy(task->resource_hold_times, resource_hold_times, N_RESOURCES);
  update_resource_ceilings(preemption_level, resource_hold_times, resource_ceilings);
}

#endif // USE_SRP
