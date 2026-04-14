#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#define TEST_SUITE_EDF 1
#define TEST_SUITE_SRP 2
#define TEST_SUITE_CBS 3
#define TEST_SUITE_MP  4

#define TEST_SUITE 2
#define TEST_NR    9

#define USE_PARTITIONED 0 // Only relevant when USE_MP=1
#define USE_GLOBAL      0 // Only relevant when USE_MP=1

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
#elif TEST_SUITE == TEST_SUITE_MP
#define USE_EDF 1
#define USE_SRP 0
#define USE_CBS 0
#define USE_MP  1
#else
#error "Invalid TEST_SUITE in ProjectConfig.h"
#endif

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

; // ==========================
; // === TEST PROFILE TABLES ===
; // ==========================

#if TEST_SUITE == TEST_SUITE_EDF
#include "config/test_profiles_edf.h"

#elif TEST_SUITE == TEST_SUITE_SRP
#include "config/test_profiles_srp.h"

#elif TEST_SUITE == TEST_SUITE_CBS
#include "config/test_profiles_cbs.h"

#elif TEST_SUITE == TEST_SUITE_MP
#include "config/test_profiles_mp.h"

#else
#error "Invalid TEST_SUITE in ProjectConfig.h"
#endif

; // =============================
; // === COMMON VALIDATION/DEF ===
; // =============================

#ifndef MAXIMUM_PERIODIC_TASKS
#error "MAXIMUM_PERIODIC_TASKS not set by selected test profile"
#endif

#ifndef MAXIMUM_APERIODIC_TASKS
#error "MAXIMUM_APERIODIC_TASKS not set by selected test profile"
#endif

#if USE_SRP
#ifndef N_RESOURCES
#error "N_RESOURCES not set for SRP profile"
#endif

#ifndef N_PREEMPTION_LEVELS
#error "N_PREEMPTION_LEVELS not set for SRP profile"
#endif

#ifndef ENABLE_STACK_SHARING
#error "ENABLE_STACK_SHARING not set for SRP profile"
#endif
#endif

#ifndef PERFORM_ADMISSION_CONTROL
#define PERFORM_ADMISSION_CONTROL 1
#endif

#ifndef TEST_DURATION_TICKS
#define TEST_DURATION_TICKS 500
#endif

#endif /* PROJECT_CONFIG_H */
