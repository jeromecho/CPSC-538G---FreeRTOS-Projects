#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include "FreeRTOS.h" // IWYU pragma: keep

#define USE_EDF 1     // TODO: Ensure that this configuration constant actually affects execution
#define USE_SRP 0
#define USE_CBS 1
#define TEST_NR 2

#define SHARED_STACK_SIZE         (configMINIMAL_STACK_SIZE)
#define MAX_TRACE_RECORDS         300
#define TRACE_WITH_LOGIC_ANALYZER 0

// clang-format off

#if USE_EDF

  #if USE_CBS
    #if TEST_NR == 1
      #define MAXIMUM_PERIODIC_TASKS  1
      #define MAXIMUM_APERIODIC_TASKS 1
    #elif TEST_NR == 2
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 1
    #elif TEST_NR == 3 
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 1
    #elif TEST_NR == 4
      #define MAXIMUM_PERIODIC_TASKS  1
      #define MAXIMUM_APERIODIC_TASKS 1
    #elif TEST_NR == 5
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 1
    #elif TEST_NR == 6
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 2
    #elif TEST_NR == 7
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 2
    #elif TEST_NR == 8
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 2
    #elif TEST_NR == 9
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 2
    #elif TEST_NR == 10
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 3
    #elif TEST_NR == 11
      #define MAXIMUM_PERIODIC_TASKS  1
      #define MAXIMUM_APERIODIC_TASKS 2
    #elif TEST_NR == 12
      #define MAXIMUM_PERIODIC_TASKS  1
      #define MAXIMUM_APERIODIC_TASKS 3
    #elif TEST_NR == 13
      #define MAXIMUM_PERIODIC_TASKS  1
      #define MAXIMUM_APERIODIC_TASKS 1
    #elif TEST_NR == 14
      #define MAXIMUM_PERIODIC_TASKS  1
      #define MAXIMUM_APERIODIC_TASKS 1
    #elif TEST_NR == 15
      #define MAXIMUM_PERIODIC_TASKS  1
      #define MAXIMUM_APERIODIC_TASKS 1
    #elif TEST_NR == 16
      #define MAXIMUM_PERIODIC_TASKS  1
      #define MAXIMUM_APERIODIC_TASKS 1
    #elif TEST_NR == 17
      #define MAXIMUM_PERIODIC_TASKS  1
      #define MAXIMUM_APERIODIC_TASKS 1
    #else
      #error "Invalid or undefined TEST_NR"
    #endif // TEST_NR

  #elif USE_SRP

    #if TEST_NR == 1
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 3
      #define N_RESOURCES             1
      #define N_PREEMPTION_LEVELS     3
      #define ENABLE_STACK_SHARING    0
    #elif TEST_NR == 2
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 4
      #define N_RESOURCES             3
      #define N_PREEMPTION_LEVELS     4
      #define ENABLE_STACK_SHARING    0
    #elif TEST_NR == 3
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 3
      #define N_RESOURCES             0
      #define N_PREEMPTION_LEVELS     2
      #define ENABLE_STACK_SHARING    0
    #elif TEST_NR == 4
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 3
      #define N_RESOURCES             0
      #define N_PREEMPTION_LEVELS     2
      #define ENABLE_STACK_SHARING    1
    #elif TEST_NR == 5
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 100
      #define N_RESOURCES             0
      #define N_PREEMPTION_LEVELS     5
      #define ENABLE_STACK_SHARING    0
    #elif TEST_NR == 6
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 100
      #define N_RESOURCES             0
      #define N_PREEMPTION_LEVELS     5
      #define ENABLE_STACK_SHARING    1
    #elif TEST_NR == 7
      #define MAXIMUM_PERIODIC_TASKS  3
      #define MAXIMUM_APERIODIC_TASKS 0
      #define N_RESOURCES             1
      #define N_PREEMPTION_LEVELS     3
      #define ENABLE_STACK_SHARING    1
    #elif TEST_NR == 8
      #define MAXIMUM_PERIODIC_TASKS  3
      #define MAXIMUM_APERIODIC_TASKS 0
      #define N_RESOURCES             1
      #define N_PREEMPTION_LEVELS     3
      #define ENABLE_STACK_SHARING    1
    #elif TEST_NR == 9
      #define MAXIMUM_PERIODIC_TASKS  3
      #define MAXIMUM_APERIODIC_TASKS 0
      #define N_RESOURCES             1
      #define N_PREEMPTION_LEVELS     3
      #define ENABLE_STACK_SHARING    1

    #else
      #error "Invalid or undefined TEST_NR"

    #endif // TEST_NR

    // Validation of definitions
    #ifndef N_RESOURCES
      #error "N_RESOURCES not set"
    #endif
    #ifndef N_PREEMPTION_LEVELS
      #error "N_PREEMPTION_LEVELS not set"
    #endif
    #ifndef ENABLE_STACK_SHARING
      #error "ENABLE_STACK_SHARING not set"
    #endif

  #else // USE_SRP

    #if TEST_NR == 1
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 0
    #elif TEST_NR == 2
      #define MAXIMUM_PERIODIC_TASKS  3
      #define MAXIMUM_APERIODIC_TASKS 0
    #elif TEST_NR == 3
      #define MAXIMUM_PERIODIC_TASKS  100
      #define MAXIMUM_APERIODIC_TASKS 0
    #elif TEST_NR == 4
      #define MAXIMUM_PERIODIC_TASKS  100
      #define MAXIMUM_APERIODIC_TASKS 0
    #elif TEST_NR == 5
      #define MAXIMUM_PERIODIC_TASKS  10
      #define MAXIMUM_APERIODIC_TASKS 0
    #elif TEST_NR == 6
      #define MAXIMUM_PERIODIC_TASKS  10
      #define MAXIMUM_APERIODIC_TASKS 0
    #elif TEST_NR == 7
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 0
    #elif TEST_NR == 8
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 0
    #elif TEST_NR == 9
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 0
    #elif TEST_NR == 10
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 0
    #elif TEST_NR == 11
      #define MAXIMUM_PERIODIC_TASKS    2
      #define MAXIMUM_APERIODIC_TASKS   0
      #define PERFORM_ADMISSION_CONTROL 0

    #else
      #error "Invalid or undefined TEST_NR"

    #endif // TEST_NR

  #endif // USE_CBS

  // Validation of definitions
  #ifndef MAXIMUM_PERIODIC_TASKS
    #error "MAXIMUM_PERIODIC_TASKS not set"
  #endif

  #ifndef MAXIMUM_APERIODIC_TASKS
    #error "MAXIMUM_APERIODIC_TASKS not set"
  #endif

  #ifndef PERFORM_ADMISSION_CONTROL
    #define PERFORM_ADMISSION_CONTROL 1
  #endif

#endif // USE_EDF

#endif /* PROJECT_CONFIG_H */
