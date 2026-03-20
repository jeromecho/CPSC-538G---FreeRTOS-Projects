#include "cbs.h"

#if USE_CBS

SchedulerCreateTask_t CBS_create_master_task = EDF_create_aperiodic_task;

/**
 * @pre this function should only be called from within the context of a CBS master task
 */
static Basetype_t CBS_master_task_out_of_tasks(CBS_MB_t *cbs_mb) { cbs_mb.tmb_handle->aperiodic.is_runnable = false; }

static void CBS_master_task(void *pvParameters) {
  SchedulerParameters_t parameters = (SchedulerParameters_t)pvParameters;
  CBS_MB_t             *pxServer   = (CBS_MB_t *)parameters.parameters_remaining;
  while (!queue_empty(pxServer->aperiodic_tasks)) {
    AperiodicTaskFunc_t fptr;
    // NB: dequeue here should succeed
    q_top(pxServer->aperiodic_tasks, &fptr);
    // TODO: nice-to-have: add error handling if calling the function pointer returns an error
    fptr();
    q_dequeue(pxServer->aperiodic_tasks, NULL);
    if (queue_empty(pxServer->aperiodic_tasks)) {
      CBS_master_task_out_of_tasks();
    }
  }
}
// NB: if we ever want to add support for seeing which specific soft real-time aperiodic task the CBS
// master task if running, we could create a shared mapping between function pointers (corresponding to
// aperiodic tasks and integers). Then we can use the integer to decide which GPIO pin to flicker on and flicker off
// as the MASTER TASK gets prioritized and deprioritized. This logic could be kept in CBS manager
// (e.g., as a field "current_gpio_pin")

BaseType_t create_cbs_server(int Qs, int Ts, int cbs_id) {
  CBS_MB_t *pxServer = &cbs_metadata_blocks[cbs_id];
  pxServer->dsk      = 0;
  pxServer->Qs       = Qs;
  pxServer->Ts       = Ts;
  pxServer->cs       = Qs;
  pxServer->is_idle  = true; // TODO might be able to remove
  q_init(
    &pxServer->aperiodic_tasks, pxServer->aperiodic_tasks_storage, sizeof(AperiodicTaskFunc_t), CBS_QUEUE_CAPACITY
  );
  CBS_create_master_task(
    CBS_master_task,
    sprintf("CBS Server %d", cbs_id),
    CBS_MASTER_STACK_SZ,
    (void *)pxServer,
    CBS_PRIORITY_NOT_RUNNING,
    &pxServer->tmb_handle
  );
  pxServer->tmb_handle->aperiodic.is_runnable = false;
};

BaseType_t CBS_create_aperiodic_task(AperiodicTaskFunc_t task_function, int cbs_server_id) {
  CBS_MB_t *pxServer = &cbs_metadata_blocks[cbs_id];
  if (q_empty(pxServer->aperiodic_tasks)) {
    pxServer->tmb_handle->aperiodic.is_runnable = true;
    // TODO: might need to be wary about doing arithmethic with TickType_t
    TickType_t current_timestamp = xTaskGetTickCount();
    if (pxServer->cs >= (pxServer->dsk - current_timestamp) * (pxServer->Qs / pxServer->Ts)) {
      pxServer->dsk = current_timestamp + pxServer->Ts;
      pxServer->cs  = pxServer->Qs;
      // TODO remove?
      // pxServer->is_idle = false;
      pxServer->tmb_handle->absolute_deadline     = pxServer->dsk;
      pxServer->tmb_handle->aperiodic.is_runnable = true;
    }
  }
  q_enqueue([cbs_server_id].aperiodic_tasks, (void *)task_function);
};

// NB: current design - soft real-time aperiodic tasks are very bare-bones, and are only a function pointer
// (no extra book-kept fields like `release_time`, `completion_time`, and `deadline` like "real"
// EDF tasks)
// NB: enqueueing structs "wrapping around" task_function could be one way to potentially
// integrate tracing into the data structures we enqueue themselves (e.g., add a field called
// `gpio_pin` to determine which GPIO pin to turn on for a particular soft real-time aperiodic task)
BaseType_t CBS_update_budget(TMB_t current_highest_priority_task) {

};

#endif // USE_CBS