#include "cbs.h"
#include <stdio.h>

#if USE_CBS

SchedulerCreateTask_t CBS_create_master_task = EDF_create_aperiodic_task;
MarkTaskDone_t        CBS_mark_task_done     = EDF_mark_task_done;

StackType_t      cbs_private_stacks_master[MAXIMUM_CBS_SERVERS][CBS_MASTER_STACK_SZ];
CBS_MB_t         cbs_metadata_blocks[MAXIMUM_CBS_SERVERS];
PendingCBSTask_t pending_cbs_tasks[MAX_PENDING_CBS_TASKS];

// TODO: Add a CBS_init function
// that initializes the is_active flag of `pending_cbs_tasks` to false

; // ===================================
; // === HELPER FUNCTION DEFINITIONS ===
; // ===================================
/**
 * @pre this function should only be called from within the context of a CBS master task
 */
static void CBS_master_task_out_of_tasks(CBS_MB_t *cbs_mb) {
  TRACE_record(EVENT_BASIC(TRACE_DONE), TRACE_TASK_APERIODIC, cbs_mb->tmb_handle, false);
  CBS_mark_task_done(cbs_mb->tmb_handle->handle);
}

static inline CBS_MB_t *find_cbs_server_by_tmb(TMB_t *task) {
  if (task == NULL)
    return NULL;

  for (size_t i = 0; i < MAXIMUM_CBS_SERVERS; ++i) {
    if (cbs_metadata_blocks[i].tmb_handle == task) {
      return &cbs_metadata_blocks[i];
    }
  }
  return NULL;
}


; // ================================
; // === API FUNCTION DEFINITIONS ===
; // ================================

static void CBS_master_task(void *pvParameters) {
  SchedulerParameters_t *parameters = (SchedulerParameters_t *)pvParameters;
  CBS_MB_t              *pxServer   = (CBS_MB_t *)parameters->parameters_remaining;
  for (;;) {
    if (q_empty(&pxServer->aperiodic_tasks)) {
      CBS_master_task_out_of_tasks(pxServer);
    }
    AperiodicTaskFunc_t fptr;
    q_top(&pxServer->aperiodic_tasks, &fptr);
    // TODO: nice-to-have: add error handling if calling the function pointer returns an error
    fptr();
    // NB: dequeue here should succeed
    q_dequeue(&pxServer->aperiodic_tasks, NULL);
  }
}

BaseType_t create_cbs_server(int Qs, int Ts, int cbs_id) {
  configASSERT(Qs > 0);
  CBS_MB_t *pxServer = &cbs_metadata_blocks[cbs_id];
  pxServer->dsk      = 0;
  pxServer->Qs       = Qs;
  pxServer->Ts       = Ts;
  pxServer->cs       = Qs;

  q_init(
    &pxServer->aperiodic_tasks,
    (void *)pxServer->aperiodic_tasks_storage,
    sizeof(AperiodicTaskFunc_t),
    CBS_QUEUE_CAPACITY
  );

  char pcTaskName[20];
  sprintf(pcTaskName, "CBS Server %d", cbs_id);

  CBS_create_master_task(
    CBS_master_task, pcTaskName, portMAX_DELAY, 0, pxServer->dsk, &pxServer->tmb_handle, (void *)pxServer, false
  );
  // Newly created CBS server should not be considered for execution
  pxServer->tmb_handle->is_done = true;
};

BaseType_t CBS_create_aperiodic_task(AperiodicTaskFunc_t task_function, int cbs_id, TickType_t release_time) {
  // Find an empty slot in the pending buffer
  for (int i = 0; i < MAX_PENDING_CBS_TASKS; i++) {
    if (!pending_cbs_tasks[i].is_active) {
      pending_cbs_tasks[i].task_function = task_function;
      pending_cbs_tasks[i].cbs_id        = cbs_id;
      pending_cbs_tasks[i].release_time  = release_time;
      pending_cbs_tasks[i].is_active     = true;
      return pdPASS;
    }
  }
  return pdFAIL; // Buffer full
};

// ASSUMPTION: CBS_update_budget is called every time slice
// INVARIANT: last_server->cs > 0 at entry of function (i.e., server capacity > 0 must hold)
BaseType_t CBS_update_budget(TMB_t *current_task) {
  const BaseType_t budget_exhausted = pdFALSE;
  CBS_MB_t        *server           = find_cbs_server_by_tmb(current_task);
  if (server == NULL) {
    return pdFALSE;
  }
  TickType_t count = xTaskGetTickCount();

  server->cs -= 1;

  if (server->cs == 0) {
    server->dsk += server->Ts;
    server->tmb_handle->absolute_deadline = server->dsk;
    server->cs                            = server->Qs;
    TRACE_record(EVENT_BASIC(TRACE_BUDGET_RUN_OUT), TRACE_TASK_APERIODIC, server->tmb_handle, true);
    return pdTRUE;
  }
  return pdFALSE;
};

void CBS_release_tasks() {
  TickType_t current_timestamp = xTaskGetTickCount();
  for (int i = 0; i < MAX_PENDING_CBS_TASKS; i++) {
    if (pending_cbs_tasks[i].is_active && current_timestamp >= pending_cbs_tasks[i].release_time) {
      int       cbs_id   = pending_cbs_tasks[i].cbs_id;
      CBS_MB_t *pxServer = &cbs_metadata_blocks[cbs_id];
      if (q_empty(&pxServer->aperiodic_tasks)) {
        if (pxServer->tmb_handle->is_done &&
            (double)pxServer->cs >=
              ((double)pxServer->dsk - (double)current_timestamp) * ((double)pxServer->Qs / (double)pxServer->Ts)) {
          pxServer->dsk                           = current_timestamp + pxServer->Ts;
          pxServer->cs                            = pxServer->Qs;
          pxServer->tmb_handle->absolute_deadline = pxServer->dsk;
        }
      }
      pxServer->tmb_handle->is_done = false;
      q_enqueue(&pxServer->aperiodic_tasks, (void *)&pending_cbs_tasks[i].task_function);
      pending_cbs_tasks[i].is_active = false;
    }
  }
}

#endif // USE_CBS
