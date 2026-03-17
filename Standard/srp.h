#ifndef SRP_H
#define SRP_H

#include "FreeRTOS.h" // IWYU pragma: keep
#include "ProjectConfig.h"
#include "scheduler_internal.h"
#include "task.h"

#if USE_SRP

#define MAX_SRP_NESTING (N_PREEMPTION_LEVELS + N_RESOURCES)

// SRP State structure
typedef struct {
  unsigned int resource_availability[N_RESOURCES];

  unsigned int priority_ceiling_stack[MAX_SRP_NESTING];
  size_t       priority_ceiling_index;
} SRPState_t;

// API Declarations
BaseType_t   SRP_take_binary_semaphore(const unsigned int semaphoreIdx);
void         SRP_give_binary_semaphore(const unsigned int semaphoreIdx);
unsigned int SRP_get_system_ceiling();

void SRP_push_ceiling(unsigned int level);
void SRP_pop_ceiling(void);

BaseType_t SRP_create_periodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  period,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  const BaseType_t  preemption_level,
  const TickType_t  resource_hold_times[N_RESOURCES]
);
BaseType_t SRP_create_aperiodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  const BaseType_t  preemption_level,
  const TickType_t  resource_hold_times[N_RESOURCES]
);

bool SRP_can_admit_periodic_task( //
  const TickType_t   completion_time,
  const TickType_t   period,
  const TickType_t   relative_deadline,
  const unsigned int preemption_level,
  const TickType_t   resource_hold_times[N_RESOURCES]
);

void SRP_reset_TCB(const TMB_t *const task);

#endif // USE_SRP

#endif // SRP_H
