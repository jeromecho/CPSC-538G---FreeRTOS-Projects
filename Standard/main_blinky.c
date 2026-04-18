/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/******************************************************************************
 * NOTE 1:  This project provides two demo applications.  A simple blinky
 * style project, and a more comprehensive test and demo application.  The
 * mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting in main.c is used to select
 * between the two.  See the notes on using mainCREATE_SIMPLE_BLINKY_DEMO_ONLY
 * in main.c.  This file implements the simply blinky style version.
 *
 * NOTE 2:  This file only contains the source code that is specific to the
 * basic demo.  Generic functions, such FreeRTOS hook functions, and functions
 * required to configure the hardware are defined in main.c.
 ******************************************************************************
 *
 * main_blinky() creates one queue, and two tasks.  It then starts the
 * scheduler.
 *
 * The Queue Send Task:
 * The queue send task is implemented by the prvQueueSendTask() function in
 * this file.  prvQueueSendTask() sits in a loop that causes it to repeatedly
 * block for 200 milliseconds, before sending the value 100 to the queue that
 * was created within main_blinky().  Once the value is sent, the task loops
 * back around to block for another 200 milliseconds...and so on.
 *
 * The Queue Receive Task:
 * The queue receive task is implemented by the prvQueueReceiveTask() function
 * in this file.  prvQueueReceiveTask() sits in a loop where it repeatedly
 * blocks on attempts to read data from the queue that was created within
 * main_blinky().  When data is received, the task checks the value of the
 * data, and if the value equals the expected 100, toggles an LED.  The 'block
 * time' parameter passed to the queue receive function specifies that the
 * task should be held in the Blocked state indefinitely to wait for data to
 * be available on the queue.  The queue receive task will only leave the
 * Blocked state when the queue send task writes to the queue.  As the queue
 * send task writes to the queue every 200 milliseconds, the queue receive
 * task leaves the Blocked state every 200 milliseconds, and therefore toggles
 * the LED every 200 milliseconds.
 */

#include "main_blinky.h"

#include "ProjectConfig.h"

/* Kernel includes. */
#include "FreeRTOS_include.h"

/* Library includes. */
#include "hardware/gpio.h"
#include <stdio.h>

#if USE_EDF

// Custom scheduler includes
#include "edf_scheduler.h"
#include "helpers.h"
#include "tracer.h"

#include "config/TestConfig.h" // IWYU pragma: keep
#if TEST_SUITE == TEST_SUITE_EDF
#include "testing/edf_tests.h" // IWYU pragma: keep

#elif TEST_SUITE == TEST_SUITE_SRP
#include "testing/srp_tests.h" // IWYU pragma: keep

#elif TEST_SUITE == TEST_SUITE_CBS
#include "testing/cbs_tests.h" // IWYU pragma: keep

#elif TEST_SUITE == TEST_SUITE_PARTITIONED_MP
#include "testing/partitioned_mp_tests.h" // IWYU pragma: keep
#endif
#endif                                    // USE_EDF

// Other includes
#include "pico/stdlib.h" // IWYU pragma: keep

#if !USE_EDF
#include "fixed_priority_support.h"
#endif

/*-----------------------------------------------------------*/

/*
 * Called by main when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 1 in
 * main.c.
 */
void main_blinky(void);

/*
 * Helpers
 */
void initialize_gpio_pins(void);
void run_test();

/*-----------------------------------------------------------*/

TaskHandle_t monitor_task_handle = NULL;

#if USE_EDF

void vTraceMonitorTask(void *pvParameters) {
  // Either sleeps for the specified duration, or is forced to wake by task notification due to something like a
  // deadline miss
  (void)pvParameters;
  ulTaskNotifyTake(pdTRUE, TEST_DURATION_TICKS);

  // The test is over, so output trace
  vTaskSuspendAll();
  TRACE_disable();

#if TEST_SUITE == TEST_SUITE_EDF
  printf("Results for EDF Test %d\n", TEST_NR);
#elif TEST_SUITE == TEST_SUITE_SRP
  printf("Results for SRP Test %d\n", TEST_NR);
#elif TEST_SUITE == TEST_SUITE_CBS
  printf("Results for CBS Test %d\n", TEST_NR);
#elif TEST_SUITE == TEST_SUITE_PARTITIONED_MP
  printf("Results for SMP Test %d\n", TEST_NR);
#elif TEST_SUITE == TEST_SUITE_GLOBAL_MP
  printf("Results for SMP Test %d\n", TEST_NR);
#endif

  crash_with_trace("");
}

void main_blinky(void) {
// Block execution until the host opens the USB serial port
#if !TRACE_WITH_LOGIC_ANALYZER
  while (!stdio_usb_connected()) {
    sleep_ms(100);
  }
#endif

  printf("Starting main_blinky.\n");
  initialize_gpio_pins();

#if !TRACE_WITH_LOGIC_ANALYZER
  // This creates the monitor task, which is responsible for printing all trace data after a certain amount of time has
  // passed. It doesn't interfere with the scheduling.
  const BaseType_t monitor_created = xTaskCreate(
    vTraceMonitorTask,
    "Monitor",
    configMINIMAL_STACK_SIZE + 256, // Give it enough stack for printf
    (void *)TEST_DURATION_TICKS,
    configMAX_PRIORITIES - 1,
    &monitor_task_handle
  );

  if (monitor_created != pdPASS) {
    vTaskSuspendAll();
    crash_without_trace("Failed to create monitor task; trace output will be unavailable.");
  }

#if USE_MP
  vTaskCoreAffinitySet(monitor_task_handle, (1 << 0));
#endif

#if (configUSE_CORE_AFFINITY == 1)
  const UBaseType_t core_affinity_mask = ((UBaseType_t)1U) << configTICK_CORE;
  vTaskCoreAffinitySet(monitor_task_handle, core_affinity_mask);
#endif // configUSE_CORE_AFFINITY
#endif // TRACE_WITH_LOGIC_ANALYZER

  run_test();

  /* Start the tasks and timer running. */
  printf("Starting scheduler.\n");
  vTaskStartScheduler();

  /* If all is well, the scheduler will now be running, and the following
  line will never be reached.  If the following line does execute, then
  there was insufficient FreeRTOS heap memory available for the Idle and/or
  timer tasks to be created.  See the memory management section on the
  FreeRTOS web site for more details on the FreeRTOS heap
  http://www.freertos.org/a00111.html. */
  for (;;)
    ;
}

void initialize_gpio_pins(void) {
  gpio_put(PICO_DEFAULT_LED_PIN, 0);
  gpio_put(mainGPIO_IDLE_TASK, 0);

  for (size_t i = mainGPIO_PERIODIC_TASK_BASE; i < mainGPIO_PERIODIC_TASK_END; i++) {
    gpio_put(i, 0);
  }
  for (size_t i = mainGPIO_APERIODIC_TASK_BASE; i < mainGPIO_APERIODIC_TASK_END; i++) {
    gpio_put(i, 0);
  }
}

#define PASTE(x, y)        x##y
#define PASTE_EXPAND(x, y) PASTE(x, y)
/// @brief Uses the PASTE_EXPAND macro to dynamically dispatch the correct test based on the current TEST_NR
void run_test() {
#if TEST_SUITE == TEST_SUITE_EDF
  printf("Running EDF Test %d\n", TEST_NR);
  PASTE_EXPAND(edf_test_, TEST_NR)();
#elif TEST_SUITE == TEST_SUITE_SRP
  printf("Running SRP Test %d\n", TEST_NR);
  PASTE_EXPAND(srp_test_, TEST_NR)();
#elif TEST_SUITE == TEST_SUITE_CBS
  printf("Running CBS Test %d\n", TEST_NR);
  PASTE_EXPAND(cbs_test_, TEST_NR)();
#elif TEST_SUITE == TEST_SUITE_PARTITIONED_MP
  printf("Running Partitioned MP Test %d\n", TEST_NR);
  PASTE_EXPAND(partitioned_mp_test_, TEST_NR)();
#endif
}
#else

#define FP_TEST_DURATION_TICKS 14

typedef struct {
  uint8_t    task_id;
  uint8_t    gpio_pin;
  TickType_t period_ticks;
} FPTaskParams_t;

static FPTaskParams_t fp_high_params = {0, mainGPIO_PERIODIC_TASK_BASE, 5};
static FPTaskParams_t fp_low_params  = {1, (mainGPIO_PERIODIC_TASK_BASE + 1), 5};

static void vFPTraceMonitorTask(void *pvParameters) {
  (void)pvParameters;

  vTaskDelay(FP_TEST_DURATION_TICKS);
  vTaskSuspendAll();

  printf("Results for FP Test %d\n", TEST_NR);
  FP_trace_print_buffer();

  for (;;) {
    __asm volatile("wfi");
  }
}

static void prvFixedPriorityTask(void *pvParameters) {
  const FPTaskParams_t *const task_params = (const FPTaskParams_t *)pvParameters;
  const uint32_t             gpio_pin     = task_params->gpio_pin;
  TickType_t                 last_wake    = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&last_wake, task_params->period_ticks);

    gpio_put(gpio_pin, 1);
    const TickType_t start_tick = xTaskGetTickCount();
    while (xTaskGetTickCount() == start_tick) {
      ;
    }
    gpio_put(gpio_pin, 0);
  }
}

void main_blinky(void) {
// Block execution until the host opens the USB serial port
#if !TRACE_WITH_LOGIC_ANALYZER
  while (!stdio_usb_connected()) {
    sleep_ms(100);
  }
#endif

  printf("Starting main_blinky in fixed-priority mode.\n");
  printf("Running FP Test %d\n", TEST_NR);
  initialize_gpio_pins();

  FP_trace_reset();

  TaskHandle_t fp_high_handle = NULL;
  TaskHandle_t fp_low_handle  = NULL;

  const BaseType_t high_task_created = xTaskCreate(
    prvFixedPriorityTask,
    "FP High",
    configMINIMAL_STACK_SIZE + 128,
    (void *)&fp_high_params,
    tskIDLE_PRIORITY + 2,
    &fp_high_handle
  );

  const BaseType_t low_task_created = xTaskCreate(
    prvFixedPriorityTask,
    "FP Low",
    configMINIMAL_STACK_SIZE + 128,
    (void *)&fp_low_params,
    tskIDLE_PRIORITY + 1,
    &fp_low_handle
  );

  TaskHandle_t monitor_handle = NULL;
  const BaseType_t monitor_created = xTaskCreate(
    vFPTraceMonitorTask,
    "FP Monitor",
    configMINIMAL_STACK_SIZE + 256,
    NULL,
    configMAX_PRIORITIES - 1,
    &monitor_handle
  );

  if (high_task_created != pdPASS || low_task_created != pdPASS || monitor_created != pdPASS) {
    printf("Failed to create fixed-priority demo tasks.\n");
    for (;;)
      ;
  }

  FP_trace_register_tasks(fp_high_handle, fp_low_handle);

  printf("Starting scheduler.\n");
  vTaskStartScheduler();

  for (;;)
    ;
}

void initialize_gpio_pins(void) {
  gpio_put(PICO_DEFAULT_LED_PIN, 0);
  gpio_put(mainGPIO_IDLE_TASK, 0);

  for (size_t i = mainGPIO_PERIODIC_TASK_BASE; i < mainGPIO_PERIODIC_TASK_END; i++) {
    gpio_put(i, 0);
  }
  for (size_t i = mainGPIO_APERIODIC_TASK_BASE; i < mainGPIO_APERIODIC_TASK_END; i++) {
    gpio_put(i, 0);
  }
}

#endif
