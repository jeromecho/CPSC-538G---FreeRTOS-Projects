#include "FreeRTOS.h" // IWYU pragma: keep

#ifndef HELPERS_H
#define HELPERS_H

TickType_t gcd(TickType_t a, TickType_t b);
TickType_t lcm(const TickType_t a, const TickType_t b);
TickType_t compute_hyperperiod(const TickType_t new_period);

void execute_for_ticks(const TickType_t n);

#endif
