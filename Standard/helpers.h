#include "FreeRTOS.h"

#ifndef HELPERS_H
#define HELPERS_H

TickType_t gcd(TickType_t a, TickType_t b);
TickType_t lcm(TickType_t a, TickType_t b);
TickType_t compute_hyperperiod(TickType_t new_period);

void execute_for_ticks(TickType_t n);

#endif
