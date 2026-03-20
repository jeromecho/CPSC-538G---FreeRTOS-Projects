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
  pxServer->is_idle  = true;
  q_init(
    &pxServer->aperiodic_tasks, pxServer->aperiodic_tasks_storage, sizeof(AperiodicTaskFunc_t), CBS_QUEUE_CAPACITY
  );

  // !!!
  // TODO: - below is not correct, it might be beneficial to have a separate array of tasks (called CBS_master_tasks)
  // that the EDF scheduler iterates over depending on whether the USE_CBS flag is enabled or not

  // NB: It might be beneficial to use feature flags to turn on and off CBS server creation functions inside of
  // `edf_scheduler.c`
  // Q: Right now, the field for server's current deadline lives inside of `cbs` - but the EDF scheduler will need this
  // field (as well as the "fullness" of the server's current queue to decide the priority of tasks)
  //   - How should this information be reported between CBS and EDF without coupling?
  CBS_create_master_task(
    CBS_master_task,
    sprintf("CBS Server %d", cbs_id),
    CBS_MASTER_STACK_SZ,
    (void *)pxServer,
    CBS_PRIORITY_NOT_RUNNING,
    NULL
  );
};

BaseType_t CBS_create_aperiodic_task(AperiodicTaskFunc_t task_function, int cbs_server_id) {
  q_enqueue([cbs_server_id].aperiodic_tasks, (void *)task_function);
};

// NB: current design - soft real-time aperiodic tasks are very bare-bones, and are only a function pointer
// (no extra book-kept fields like `release_time`, `completion_time`, and `deadline` like "real"
// EDF tasks)
// NB: enqueueing structs "wrapping around" task_function could be one way to potentially
// integrate tracing into the data structures we enqueue themselves (e.g., add a field called
// `gpio_pin` to determine which GPIO pin to turn on for a particular soft real-time aperiodic task)
BaseType_T CBS_update_budget(TMB_t current_highest_priority_task) {

};

#endif // USE_CBS