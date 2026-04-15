#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

#include "ProjectConfig.h"

; // =====================
; // === TEST PROFILES ===
; // =====================


#include "config/test_profiles_cbs.h"            // IWYU pragma: keep
#include "config/test_profiles_edf.h"            // IWYU pragma: keep
#include "config/test_profiles_global_mp.h"      // IWYU pragma: keep
#include "config/test_profiles_partitioned_mp.h" // IWYU pragma: keep
#include "config/test_profiles_srp.h"            // IWYU pragma: keep


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

#endif // TEST_CONFIG_H
