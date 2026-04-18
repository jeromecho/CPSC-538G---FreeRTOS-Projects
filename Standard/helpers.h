#ifndef HELPERS_H
#define HELPERS_H

#include "ProjectConfig.h"

#if USE_EDF

#include "types/scheduler_types.h"

TickType_t gcd(TickType_t a, TickType_t b);
TickType_t lcm(const TickType_t a, const TickType_t b);
TickType_t compute_hyperperiod(const TickType_t new_period, const TMB_t *tasks_array, const size_t array_size);

void crash_without_trace(const char *format, ...);
void crash_with_trace(const char *format, ...);

#endif // USE_EDF

#endif
