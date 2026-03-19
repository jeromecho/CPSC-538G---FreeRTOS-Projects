#include "cbs.h"

#if USE_CBS

SchedulerCreateTask_t CBS_create_master_task = EDF_create_aperiodic_task;

static void CBS_master_task(void *pvParameters) {
  CBS_MB_t *pxServer = (CBS_MB_t *)pvParameters;
  while (!queue_empty(pxServer->aperiodic_tasks)) {
    AperiodicTaskFunc_t fptr;
    // NB: dequeue here should succeed
    q_top(pxServer->aperiodic_tasks, &fptr);
    // TODO: nice-to-have: add error handling if calling the function pointer returns an error
    fptr();
    q_dequeue(pxServer->aperiodic_tasks, NULL);
  }
}

BaseType_t create_cbs_server(int Qs, int Ts, int cbs_id) {
  CBS_MB_t *pxServer = &cbs_metadata_blocks[cbs_id];
  pxServer->dsk      = 0;
  pxServer->Qs       = Qs;
  pxServer->Ts       = Ts;
  pxServer->is_idle  = true;
  q_init(
    &pxServer->aperiodic_tasks, pxServer->aperiodic_tasks_storage, sizeof(AperiodicTaskFunc_t), CBS_QUEUE_CAPACITY
  );
  CBS_create_master_task(
    CBS_master_task,
    sprintf("CBS Server %d", cbs_id),
    CBS_MASTER_STACK_SZ,
    (void *)pxServer,
    CBS_PRIORITY_NOT_RUNNING,
    NULL
  );
};

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