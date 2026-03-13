#include "FreeRTOS.h" // IWYU pragma: keep
#include "edf_scheduler.h"

#ifndef HELPERS_H
#define HELPERS_H

TickType_t gcd(TickType_t a, TickType_t b);
TickType_t lcm(const TickType_t a, const TickType_t b);
TickType_t compute_hyperperiod(const TickType_t new_period, const TMB_t *tasks_array, const size_t array_size);

void execute_for_ticks(const TickType_t n);

#endif
