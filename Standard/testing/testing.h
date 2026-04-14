#ifndef TESTING_H
#define TESTING_H

#include "FreeRTOS.h" // IWYU pragma: keep

/// @brief The relevant parameters for periodic tasks
typedef struct {
  TaskFunction_t func; // Function pointer
  TickType_t     C;    // Completion time
  TickType_t     T;    // Period
  TickType_t     D;    // Relative deadline

#if USE_SRP
  UBaseType_t plvl;                   // Preemption Level
  TickType_t  resources[N_RESOURCES]; // Hold times for different resources
#endif

#if USE_MP
  uint8_t core; // Preferred core for the task
#endif
} PeriodicTaskParams_t;

/// @brief The relevant parameters for aperiodic tasks
typedef struct {
  TaskFunction_t func; // Function pointer
  TickType_t     C;    // Completion time
  TickType_t     r;    // Release time
  TickType_t     D;    // Relative deadline

#if USE_SRP
  UBaseType_t plvl;                   // Preemption Level
  TickType_t  resources[N_RESOURCES]; // Hold times for different resources
#endif

#if USE_MP
  uint8_t core; // Preferred core for the task
#endif
} AperiodicTaskParams_t;

/// @brief Different actions a task can take when setting up a simple task model
typedef enum {
#if USE_SRP
  TASK_TAKE_SEMAPHORE,
  TASK_GIVE_SEMAPHORE,
#endif
  TASK_EXECUTE
} TaskAction_t;

/// @brief Collection of an action and any corresponding data necessary to complete that action
typedef struct {
  TaskAction_t action;
  union {
    int duration;
    int semaphore_index;
  };
} TaskStep_t;

#define LEN(x) (sizeof(x) / sizeof((x)[0]))

void build_periodic_task(const char *task_name, const PeriodicTaskParams_t *config);
void build_aperiodic_task(const char *task_name, const AperiodicTaskParams_t *config);
void build_periodic_test(const char *test_name, const PeriodicTaskParams_t *config, size_t num_tasks);
void build_aperiodic_test(const char *test_name, const AperiodicTaskParams_t *config, size_t num_tasks);
void execute_steps(const TickType_t completion_time, const TaskStep_t steps[], const size_t num_steps);

#endif // TESTING_H
