#ifndef FIXED_PRIORITY_SUPPORT_H
#define FIXED_PRIORITY_SUPPORT_H

#include "ProjectConfig.h"

#if !USE_EDF

#include "FreeRTOS_include.h"

void FP_trace_reset(void);
void FP_trace_register_tasks(TaskHandle_t high_task, TaskHandle_t low_task);
void FP_trace_print_buffer(void);

#endif // !USE_EDF

#endif // FIXED_PRIORITY_SUPPORT_H
