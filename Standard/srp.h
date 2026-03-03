#ifndef SRP_H
#define SRP_H

#include "FreeRTOS.h"
#include "task.h"

// Client must define this, or it can be passed via a config file
#define N_RESOURCES 3

// TMF structure as defined in your design
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
void vSRP_Initialize(TMF_t *task_matrix, size_t num_tasks, unsigned int *user_ceilings_memory);
BaseType_t   vBinSempahoreTakeSRP(unsigned int semaphoreIdx);
void         vBinSemaphoreGiveSRP(unsigned int semaphoreIdx);
unsigned int get_srp_system_ceiling(void);
bool         srp_is_initialized();

#endif // SRP_H
