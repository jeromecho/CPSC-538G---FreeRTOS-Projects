#ifndef SRP_H
#define SRP_H

#include "FreeRTOS.h" // IWYU pragma: keep
#include "ProjectConfig.h"
#include "edf_scheduler.h"
#include "task.h"

#if USE_SRP

// TMF structure as defined in design document
typedef struct {
  unsigned int           preemption_level;
  unsigned int           resource_hold_times[N_RESOURCES];
  configSTACK_DEPTH_TYPE stackSize;
} TMF_t;

// SRP State structure
typedef struct {
  bool         initialized;
  unsigned int global_priority_ceiling;
  unsigned int resource_availability[N_RESOURCES];
} SRPState_t;

// Stack element to keep track of previous states
typedef struct {
  unsigned int previous_global_ceiling;
  unsigned int semaphore_idx;
} SRP_Stack_Element_t;

// API Declarations
void SRP_initialize(TMF_t *const task_matrix, const size_t num_tasks, const unsigned int *const user_ceilings_memory);
BaseType_t   SRP_take_binary_semaphore(const unsigned int semaphoreIdx);
void         SRP_give_binary_semaphore(const unsigned int semaphoreIdx);
unsigned int SRP_get_system_ceiling();
bool         SRP_initialized();

BaseType_t SRP_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  const BaseType_t  preemption_level
);
BaseType_t SRP_create_aperiodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completionTime,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  const BaseType_t  preemption_level
);

#endif // USE_SRP

#endif // SRP_H
