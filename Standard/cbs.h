#ifndef CBS_H
#define CBS_H

#include "FreeRTOS.h"              // IWYU pragma: keep
#include "ProjectConfig.h"
#include "data_structures/queue.h" // IWYU pragma: keep

#if USE_CBS

typedef BaseType_t (*AperiodicTaskFunc_t)(void);

typedef struct {
  TickType_t dsk;
  TickType_t Qs;
  TickType_t Ts;
  TickType_t cs;              // budget
  Queue_t    aperiodic_tasks; // TODO: to implement
} CBS_MB_t;

size_t   MAXIMUM_CBS_SERVERS = 10;
size_t   CBS_QUEUE_CAPACITY  = 10;
CBS_MB_t cbs_metadata_blocks[MAXIMUM_CBS_SERVERS];

/**
 * @pre 0 <= cbs_id < MAXIMUM_CBS_SERVERS
 * @pre user's responsibility to ensure `cbs_id` is not already used by an existing server
 */
BaseType_T create_cbs_server(int Qs, int Ts, int cbs_id);

BaseType_t CBS_create_aperiodic_task(
  TaskFunction_t    task_function,
  const char *const task_name,
  const TickType_t  completion_time,
  const TickType_t  release_time,
  const TickType_t  relative_deadline,
  TMB_t **const     TMB_handle,
  int               cbs_server_id
);

#endif // USE_CBS

#endif // CBS_H