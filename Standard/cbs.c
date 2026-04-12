#include "cbs.h"
#include <stdio.h>

#if USE_CBS

SchedulerCreateTask_t CBS_create_master_task = EDF_create_aperiodic_task;
MarkTaskDone_t        CBS_mark_task_done     = EDF_mark_task_done;

StackType_t cbs_private_stacks_master[MAXIMUM_CBS_SERVERS][CBS_MASTER_STACK_SZ];
CBS_MB_t    cbs_metadata_blocks[MAXIMUM_CBS_SERVERS];

// === HELPER FUNCTION DEFINITIONS ===
// ================================
/**
 * @pre this function should only be called from within the context of a CBS master task
 */
static void CBS_master_task_out_of_tasks(CBS_MB_t *cbs_mb) {
  // TODO: Might be able to remove the critical section below
  taskENTER_CRITICAL();
  TRACE_record(EVENT_BASIC(TRACE_DONE), TRACE_TASK_APERIODIC, cbs_mb->tmb_handle);
  CBS_mark_task_done(cbs_mb->tmb_handle->handle);
  taskEXIT_CRITICAL();
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

// === API FUNCTION DEFINITIONS ===
// ================================
static void CBS_master_task(void *pvParameters) {
  SchedulerParameters_t *parameters = (SchedulerParameters_t *)pvParameters;
  CBS_MB_t              *pxServer   = (CBS_MB_t *)parameters->parameters_remaining;
  for (;;) {
    if (q_empty(&pxServer->aperiodic_tasks)) {
      CBS_master_task_out_of_tasks(pxServer);
    }
    AperiodicTaskFunc_t fptr;
    // NB: dequeue here should succeed
    q_top(&pxServer->aperiodic_tasks, &fptr);
    // TODO: nice-to-have: add error handling if calling the function pointer returns an error
    fptr();
    q_dequeue(&pxServer->aperiodic_tasks, NULL);
  }
}
// NB: if we ever want to add support for seeing which specific soft real-time aperiodic task the CBS
// master task if running, we could create a shared mapping between function pointers (corresponding to
// aperiodic tasks and integers). Then we can use the integer to decide which GPIO pin to flicker on and flicker off
// as the MASTER TASK gets prioritized and deprioritized. This logic could be kept in CBS manager
// (e.g., as a field "current_gpio_pin")

BaseType_t create_cbs_server(int Qs, int Ts, int cbs_id) {
  configASSERT(Qs > 0);
  CBS_MB_t *pxServer = &cbs_metadata_blocks[cbs_id];
  // NB: current implementation of CBS assues CBS servers are always initialized at beginning
  pxServer->dsk     = 0;
  pxServer->Qs      = Qs;
  pxServer->Ts      = Ts;
  pxServer->cs      = Qs;
  pxServer->is_idle = true; // TODO might be able to remove

  q_init(
    &pxServer->aperiodic_tasks,
    (void *)pxServer->aperiodic_tasks_storage,
    sizeof(AperiodicTaskFunc_t),
    CBS_QUEUE_CAPACITY
  );

  char pcTaskName[20];
  sprintf(pcTaskName, "CBS Server %d", cbs_id);

  CBS_create_master_task(
    CBS_master_task,
    pcTaskName,
    portMAX_DELAY,
    0, // NB: fix release time to 0 to ensure release time <= relative deadline holds
    pxServer->dsk,
    &pxServer->tmb_handle,
    (void *)pxServer,
    false
  );
  // Newly created CBS server is considered "done" since it has no tasks to execute
  // and should not be considered for execution
  pxServer->tmb_handle->is_done = true;
  // printf("create_cbs_server - CBS_create_master_task post \n");
};

BaseType_t CBS_create_aperiodic_task(AperiodicTaskFunc_t task_function, int cbs_id) {
  CBS_MB_t *pxServer = &cbs_metadata_blocks[cbs_id];
  if (q_empty(&pxServer->aperiodic_tasks)) {
    // printf("CBS_create_aperiodic_task - setting pxServer->tmb_handle->aperiodic.is_runnnable to true\n");
    // printf("CBS_create_aperiodic_task - calling vTaskResume...\n");
    vTaskResume(pxServer->tmb_handle->handle);
    // TODO: might need to be wary about doing arithmethic with TickType_t
    TickType_t current_timestamp = xTaskGetTickCount();
    if (
      pxServer->tmb_handle->is_done && (double)pxServer->cs >= ((double)pxServer->dsk - (double)current_timestamp) *
                                                                 ((double)pxServer->Qs / (double)pxServer->Ts)
    ) {
      pxServer->dsk = current_timestamp + pxServer->Ts;
      // printf("Tick: %d\n", current_timestamp);
      // printf("CBS_create_aperiodic_task - set new deadline and refill pxServer->cs\n");

      pxServer->cs = pxServer->Qs;
      // TODO remove?
      // pxServer->is_idle = false;
      pxServer->tmb_handle->absolute_deadline = pxServer->dsk;
      pxServer->tmb_handle->is_done           = false;
    }
    // printf("CBS_create_aperiodic_task: set tmb_handle->absolute_deadline to %d\n", pxServer->dsk);
    // printf("CBS_create_aperiodic_task: set is_done to false\n");
  }

  // printf("create_aperiodic_task: calling q_enqueue: task_function %d \n", task_function);
  q_enqueue(&cbs_metadata_blocks[cbs_id].aperiodic_tasks, (void *)&task_function);
};

// NB: current design - soft real-time aperiodic tasks are very bare-bones, and are only a function pointer
// (no extra book-kept fields like `release_time`, `completion_time`, and `deadline` like "real"
// EDF tasks)
// NB: enqueueing structs "wrapping around" task_function could be one way to potentially
// integrate tracing into the data structures we enqueue themselves (e.g., add a field called
// `gpio_pin` to determine which GPIO pin to turn on for a particular soft real-time aperiodic task)

// ASSUMPTION: current_task is task GURANTEED to run this current time slice
// ASSUMPTION: CBS_update_budget is called every time slice
// INVARIANT: last_server->cs > 0 at entry of function (i.e., server capacity > 0 must hold)
BaseType_t CBS_update_budget(TMB_t *current_task) {
  const BaseType_t budget_exhausted = pdFALSE;
  CBS_MB_t        *server           = find_cbs_server_by_tmb(current_task);
  if (server == NULL) {
    return pdFALSE;
  }
  TickType_t count = xTaskGetTickCount();
  // printf("Tick %d: server->cs - pre: %lu\n", count, server->cs);
  server->cs -= 1;
  // printf("Tick %d: server->cs - post: %lu\n", count, server->cs);

  if (server->cs == 0) {
    server->dsk += server->Ts;
    TRACE_record(EVENT_BASIC(TRACE_BUDGET_RUN_OUT), TRACE_TASK_APERIODIC, server->tmb_handle);
    // REQUIRES: CBS_update_budget must be called AFTER choosing of highest_priority_task
    server->tmb_handle->absolute_deadline = server->dsk;
    server->cs                            = server->Qs;
    return pdTRUE;
  }
  // printf("Tick %d: end\n", count);
  return pdFALSE;
};

#endif // USE_CBS