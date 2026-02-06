
enum Task_Type { PERIODIC, APERIODIC, N_TYPES };

typedef struct TaskSpec {
  char      *name;
  TickType_t completion_time;
  TickType_t period;
  TickType_t deadline;     // TODO - relative deadline: to use
  TickType_t release_time; // TODO - perhaps our model should support release times at variable time
                           // Logic of "releasing" a task only at or after its release time
                           // is likely going to be offloaded to the `tick interrupt hook` functions
  uint       gpio_pin;
  enum Task_Type task_type;
} TaskSpec_t;

typedef struct TestSpec {
  TaskSpec_t *tasks;
  size_t      n_tasks;
} TestSpec_t;