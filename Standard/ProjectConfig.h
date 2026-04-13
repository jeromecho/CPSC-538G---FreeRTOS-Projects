#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#define USE_EDF         1 // TODO: Ensure that this configuration constant actually affects execution
#define USE_SRP         0
#define USE_MP          1
#define USE_PARTITIONED 1
#define USE_GLOBAL      0
#define TEST_NR         2

#define SHARED_STACK_SIZE         (configMINIMAL_STACK_SIZE)
#define MAX_TRACE_RECORDS         1000
#define TRACE_WITH_LOGIC_ANALYZER 0

// Debugging
#define ENABLE_ALL_TESTS 0

// clang-format off

#if USE_EDF

  #if USE_SRP

    #if TEST_NR == 1
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 3
      #define N_RESOURCES             1
      #define N_PREEMPTION_LEVELS     3
      #define ENABLE_STACK_SHARING    0
      #define TEST_DURATION_TICKS     300
    #elif TEST_NR == 2
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 4
      #define N_RESOURCES             3
      #define N_PREEMPTION_LEVELS     4
      #define ENABLE_STACK_SHARING    0
      #define TEST_DURATION_TICKS     1500
    #elif TEST_NR == 3
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 3
      #define N_RESOURCES             0
      #define N_PREEMPTION_LEVELS     2
      #define ENABLE_STACK_SHARING    0
      #define TEST_DURATION_TICKS     300
    #elif TEST_NR == 4
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 3
      #define N_RESOURCES             0
      #define N_PREEMPTION_LEVELS     2
      #define ENABLE_STACK_SHARING    1
      #define TEST_DURATION_TICKS     300
    #elif TEST_NR == 5
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 100
      #define N_RESOURCES             0
      #define N_PREEMPTION_LEVELS     5
      #define ENABLE_STACK_SHARING    0
      #define TEST_DURATION_TICKS     1000
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
      #define TEST_DURATION_TICKS     300
    #elif TEST_NR == 8
      #define MAXIMUM_PERIODIC_TASKS  3
      #define MAXIMUM_APERIODIC_TASKS 0
      #define N_RESOURCES             1
      #define N_PREEMPTION_LEVELS     3
      #define ENABLE_STACK_SHARING    1
      #define TEST_DURATION_TICKS     300
    #elif TEST_NR == 9
      #define MAXIMUM_PERIODIC_TASKS  3
      #define MAXIMUM_APERIODIC_TASKS 0
      #define N_RESOURCES             1
      #define N_PREEMPTION_LEVELS     3
      #define ENABLE_STACK_SHARING    300

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
      #define TEST_DURATION_TICKS     11
    #elif TEST_NR == 2
      #define MAXIMUM_PERIODIC_TASKS  3
      #define MAXIMUM_APERIODIC_TASKS 0
      #define TEST_DURATION_TICKS     23
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
      #define TEST_DURATION_TICKS     400
    #elif TEST_NR == 8
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 0
    #elif TEST_NR == 9
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 0
      #define TEST_DURATION_TICKS     1200
    #elif TEST_NR == 10
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 0
      #define TEST_DURATION_TICKS     1000
    #elif TEST_NR == 11
      #define MAXIMUM_PERIODIC_TASKS    2
      #define MAXIMUM_APERIODIC_TASKS   0
      #define PERFORM_ADMISSION_CONTROL 0
      #define TEST_DURATION_TICKS       250

    #else
      #error "Invalid or undefined TEST_NR"

    #endif // TEST_NR

  #endif // USE_SRP

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

#ifndef TEST_DURATION_TICKS
  #define TEST_DURATION_TICKS 500
#endif

#endif /* PROJECT_CONFIG_H */


; // =====================
; // === SANITY CHECKS ===
; // =====================

#if USE_MP
  #if USE_PARTITIONED && USE_GLOBAL
  #error "Only one of the partitioning strategies can be active at a time"
  #endif

  #if USE_SRP
  #error "SRP and multiprocessing are not compatible"
  #endif
#endif // USE_MP

#if USE_SRP && !USE_EDF
#error "SRP relies on the EDF scheduler, and must not be active without it"
#endif
