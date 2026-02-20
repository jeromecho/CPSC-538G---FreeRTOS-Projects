#include "FreeRTOS.h"

#ifndef HELPERS_H
#define HELPERS_H

TickType_t gcd(TickType_t a, TickType_t b);

TickType_t lcm(TickType_t a, TickType_t b);

/// @brief computes hyperperiod between existing periods and period of newly added task
TickType_t compute_hyperperiod(TickType_t new_period);

#endif