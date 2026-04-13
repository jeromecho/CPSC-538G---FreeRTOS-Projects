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

  printf("Tick %d: server %d\n", count, server);
  // printf("Tick %d: server->cs - pre: %lu\n", count, server->cs);
  server->cs -= 1;
  // printf("Tick %d: server->cs - post: %lu\n", count, server->cs);

  if (server->cs == 0) {
    printf("%d += %d\n", server->dsk, server->Ts);
    server->dsk += server->Ts;
    // REQUIRES: CBS_update_budget must be called AFTER choosing of highest_priority_task
    server->tmb_handle->absolute_deadline = server->dsk;
    server->cs                            = server->Qs;
    TRACE_record(EVENT_BASIC(TRACE_BUDGET_RUN_OUT), TRACE_TASK_APERIODIC, server->tmb_handle);
    return pdTRUE;
  }
  // printf("Tick %d: end\n", count);
  return pdFALSE;
};

void CBS_release_tasks() {
  TickType_t current_timestamp = xTaskGetTickCount();
  for (int i = 0; i < MAX_PENDING_CBS_TASKS; i++) {
    if (pending_cbs_tasks[i].is_active && current_timestamp >= pending_cbs_tasks[i].release_time) {
      int       cbs_id   = pending_cbs_tasks[i].cbs_id;
      CBS_MB_t *pxServer = &cbs_metadata_blocks[cbs_id];
      // --- ORIGINAL CBS LOGIC START ---
      if (q_empty(&pxServer->aperiodic_tasks)) {
        // CBS Rule: If server is idle, check if we can reuse current deadline or must generate new one
        if (
          pxServer->tmb_handle->is_done && (double)pxServer->cs >= ((double)pxServer->dsk - (double)current_timestamp) *
                                                                     ((double)pxServer->Qs / (double)pxServer->Ts)
        ) {

          printf(
            "%f >= (%f - %f) * (%f / %f)\n",
            (double)pxServer->cs,
            (double)pxServer->dsk,
            (double)current_timestamp,
            (double)pxServer->Qs,
            (double)pxServer->Ts
          );
          pxServer->dsk                           = current_timestamp + pxServer->Ts;
          pxServer->cs                            = pxServer->Qs;
          pxServer->tmb_handle->absolute_deadline = pxServer->dsk;
        }
      }
      pxServer->tmb_handle->is_done = false;
      // Enroll the task into the server's functional queue
      q_enqueue(&pxServer->aperiodic_tasks, (void *)&pending_cbs_tasks[i].task_function);
      // --- ORIGINAL CBS LOGIC END ---
      // Mark slot as free for future requests
      pending_cbs_tasks[i].is_active = false;
    }
  }
}

#endif // USE_CBS