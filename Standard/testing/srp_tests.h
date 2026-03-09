#ifndef SRP_TESTS_H
#define SRP_TESTS_H

#include "FreeRTOS.h" // IWYU pragma: keep
#include "ProjectConfig.h"

#if USE_SRP

TickType_t srp_test_1();
TickType_t srp_test_2();

#endif // USE_SRP

#endif // SRP_TESTS_H
