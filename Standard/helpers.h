#include "FreeRTOS.h"

#ifndef HELPERS_H
#define HELPERS_H

/// @brief computes hyperperiod between existing periods and period of newly added task
static TickType_t compute_hyperperiod(TickType_t new_period);

#endif