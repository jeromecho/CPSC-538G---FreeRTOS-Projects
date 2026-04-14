#ifndef TEST_PROFILES_MP_H
#define TEST_PROFILES_MP_H

#ifdef TEST_NR

#if TEST_NR == 1
#define MAXIMUM_PERIODIC_TASKS  4
#define MAXIMUM_APERIODIC_TASKS 0
#define TEST_DURATION_TICKS     11
#else
#error "Invalid or undefined TEST_NR for EDF profile"
#endif

#endif // TEST_NR

#endif // TEST_PROFILES_MP_H
