#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#define TEST_SUITE_EDF            1
#define TEST_SUITE_SRP            2
#define TEST_SUITE_CBS            3
#define TEST_SUITE_PARTITIONED_MP 4
#define TEST_SUITE_GLOBAL_MP      5

#define TEST_SUITE 3
#define TEST_NR    3

#define SHARED_STACK_SIZE         (configMINIMAL_STACK_SIZE)
#define MAX_TRACE_RECORDS         1000
#define TRACE_WITH_LOGIC_ANALYZER 0

#if TEST_SUITE == TEST_SUITE_EDF
#define USE_EDF 1
#define USE_SRP 0
#define USE_CBS 0
#define USE_MP  0
#elif TEST_SUITE == TEST_SUITE_SRP
#define USE_EDF 1
#define USE_SRP 1
#define USE_CBS 0
#define USE_MP  0
#elif TEST_SUITE == TEST_SUITE_CBS
#define USE_EDF 1
#define USE_SRP 0
#define USE_CBS 1
#define USE_MP  0
#elif TEST_SUITE == TEST_SUITE_PARTITIONED_MP
#define USE_EDF         1
#define USE_SRP         0
#define USE_CBS         0
#define USE_MP          1
#define USE_PARTITIONED 1
#define USE_GLOBAL      0
#elif TEST_SUITE == TEST_SUITE_GLOBAL_MP
#define USE_EDF         1
#define USE_SRP         0
#define USE_CBS         0
#define USE_MP          1
#define USE_PARTITIONED 0
#define USE_GLOBAL      1
#else
#error "Invalid TEST_SUITE in ProjectConfig.h"
#endif

#ifndef USE_PARTITIONED
#define USE_PARTITIONED 0
#endif // USE_PARTITIONED

#ifndef USE_GLOBAL
#define USE_GLOBAL 0
#endif // USE_GLOBAL


; // =====================
; // === SANITY CHECKS ===
; // =====================

#if USE_SRP && !USE_EDF
#error "SRP relies on EDF and requires USE_EDF=1"
#endif

#if USE_SRP && USE_CBS
#error "SRP and CBS should not be enabled simultaneously"
#endif

#if USE_SRP && USE_MP
#error "SRP and multiprocessing are not compatible"
#endif

#if USE_CBS && USE_MP
#error "CBS and multiprocessing are not compatible"
#endif

#if USE_MP
#if USE_PARTITIONED && USE_GLOBAL
#error "Only one of USE_PARTITIONED and USE_GLOBAL can be active"
#endif

#if !USE_PARTITIONED && !USE_GLOBAL
#error "When USE_MP=1, one of USE_PARTITIONED or USE_GLOBAL must be active"
#endif
#endif

#endif /* PROJECT_CONFIG_H */
