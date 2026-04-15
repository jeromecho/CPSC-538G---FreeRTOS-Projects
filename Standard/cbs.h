#ifndef CBS_H
#define CBS_H

#include "FreeRTOS.h" // IWYU pragma: keep
#include "ProjectConfig.h"
#include "data_structures/queue.h"
#include "edf_scheduler.h"
#include "tracer.h"

#if USE_CBS

#define CBS_MASTER_STACK_SZ configMINIMAL_STACK_SIZE
#define MAXIMUM_CBS_SERVERS 10
#define CBS_QUEUE_CAPACITY  10

#define CBS_PRIORITY_NOT_RUNNING PRIORITY_NOT_RUNNING
typedef BaseType_t (*AperiodicTaskFunc_t)(void);

typedef struct {
  AperiodicTaskFunc_t task_function;
  int                 cbs_id;
  TickType_t          release_time;
  bool                is_active;
} PendingCBSTask_t;

#define MAX_PENDING_CBS_TASKS 10
extern PendingCBSTask_t pending_cbs_tasks[MAX_PENDING_CBS_TASKS];

typedef BaseType_t (*SchedulerCreateTask_t)(
  TaskFunction_t,
  const char *const,
  const TickType_t,
  const TickType_t,
  const TickType_t,
  TMB_t **const,
  void *,
  bool is_hard_rt
);

typedef void (*MarkTaskDone_t)(TaskHandle_t task_handle);

extern SchedulerCreateTask_t CBS_create_master_task;
extern MarkTaskDone_t        CBS_mark_task_done;

extern StackType_t cbs_private_stacks_master[MAXIMUM_CBS_SERVERS][CBS_MASTER_STACK_SZ];
typedef struct {
  TickType_t          dsk;
  TickType_t          Qs;
  TickType_t          Ts;
  TickType_t          cs; // budget
  Queue_t             aperiodic_tasks;
  AperiodicTaskFunc_t aperiodic_tasks_storage[CBS_QUEUE_CAPACITY];
  TMB_t              *tmb_handle;
} CBS_MB_t;

extern CBS_MB_t cbs_metadata_blocks[MAXIMUM_CBS_SERVERS];

/**
 * @pre 0 <= cbs_id < MAXIMUM_CBS_SERVERS
 * @pre user's responsibility to ensure `cbs_id` is not already used by an existing server
 */
BaseType_t create_cbs_server(int Qs, int Ts, int cbs_id);

BaseType_t CBS_create_aperiodic_task(AperiodicTaskFunc_t task_function, int cbs_server_id, TickType_t release_time);

// TOOD: not sure if "TMB_t" is generic enough to warrant as a type passed into
// a public method of `CBS` - although if all schedulers use this generic
// `TMB_t`, then the logic below could make sense
/**
 * @brief return pdTrue if budget was exhausted, false otherwise
 */
BaseType_t CBS_update_budget(TMB_t *current_highest_priority_task);

/**
 * @brief release all CBS aperiodic tasks that have their corresponding release time right now
 */
void CBS_release_tasks();

#endif // USE_CBS

#endif // CBS_H
