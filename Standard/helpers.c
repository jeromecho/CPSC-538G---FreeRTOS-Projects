#include "ProjectConfig.h"

#if USE_EDF

#include "helpers.h"

#include "tracer.h"

#include <stdarg.h>
#include <stdio.h>

TickType_t gcd(TickType_t a, TickType_t b) {
  while (b != 0) {
    const TickType_t tmp = b;
    b                    = a % b;
    a                    = tmp;
  }
  return a;
}

TickType_t lcm(const TickType_t a, const TickType_t b) { return (a / gcd(a, b)) * b; }

/// @brief Computes hyperperiod between existing periods and period of newly added task.
/// Defined in the book Hard Real-Time Computing Systems, Fourth Edition, on page 71 (Section 4.1 - Introduction)
TickType_t compute_hyperperiod(const TickType_t new_period, const TMBViewSet_t *task_view_set) {
  TickType_t H = new_period;

  if (task_view_set == NULL || task_view_set->view == NULL) {
    return H;
  }

  for (size_t i = 0; i < task_view_set->count; i++) {
    const TMB_t *task = task_view_set->view[i];
    if (task == NULL || task->handle == NULL) {
      continue;
    }
    H = lcm(H, task->periodic.period);
  }

  return H;
}

/// @brief Crash the system with a custom message printed over UART
void crash_without_trace(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  printf("\n");
  va_end(args);

  // Spin forever while putting the CPU in a low-power state.
  // This allows interrupts (like USB) to still trigger and wake the CPU
  // to service them before it returns to sleep in this loop.
  while (1) {
    __asm volatile("wfi");
  }
}

/// @brief Crash the system with a custom message printed over UART
void crash_with_trace(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  printf("\n");
  va_end(args);

  // This function is used from both task and ISR contexts (e.g., deadline_miss in vApplicationTickHook).
  // Avoid non-ISR-safe scheduler APIs here so trace dumping always executes.
  TRACE_print_buffer();

  // Spin forever while putting the CPU in a low-power state.
  // This allows interrupts (like USB) to still trigger and wake the CPU
  // to service them before it returns to sleep in this loop.
  while (1) {
    __asm volatile("wfi");
  }
}

#endif // USE_EDF
