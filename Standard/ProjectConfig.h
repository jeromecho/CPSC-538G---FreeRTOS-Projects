// clang-format off

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#define USE_EDF 1 // TODO: Ensure that this configuration constant actually affects execution

#define USE_SRP 0
#define TEST_NR 11

#define MAX_TRACE_RECORDS         300
#define TRACE_WITH_LOGIC_ANALYZER false

#if USE_EDF

  #if USE_SRP

    #if TEST_NR == 1
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 3
      #define N_RESOURCES             3
    #elif TEST_NR == 2
      #define MAXIMUM_PERIODIC_TASKS  0
      #define MAXIMUM_APERIODIC_TASKS 4
      #define N_RESOURCES             3

    #else
      #error "Invalid or undefined TEST_NR"

    #endif

    // Validation of definitions
    #ifndef N_RESOURCES
      #error "Missing N_RESOURCES definition"
    #endif

  #else

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
      #define MAXIMUM_PERIODIC_TASKS  2
      #define MAXIMUM_APERIODIC_TASKS 0

    #else
      #error "Invalid or undefined TEST_NR"

    #endif

  #endif

  // Validation of definitions
  #ifndef MAXIMUM_PERIODIC_TASKS
    #error "Missing MAXIMUM_PERIODIC_TASKS definition"
  #endif

  #ifndef MAXIMUM_APERIODIC_TASKS
    #error "Missing MAXIMUM_APERIODIC_TASKS definition"
  #endif

#endif // USE_EDF

#endif /* PROJECT_CONFIG_H */
