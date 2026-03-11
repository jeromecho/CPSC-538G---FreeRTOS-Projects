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

/* Kernel includes. */
#include "FreeRTOS.h" // IWYU pragma: keep
#include "semphr.h"
#include "task.h"

/* Library includes. */
#include "hardware/gpio.h"
#include <stdio.h>

// Custom scheduler includes
#include "edf_scheduler.h"
#include "tracer.h"

#if !USE_SRP
#include "testing/edf_tests.h" // IWYU pragma: keep
#endif                         // USE_SRP

#if USE_SRP
#include "testing/srp_tests.h" // IWYU pragma: keep
#endif                         // USE_SRP

// Other includes
#include "pico/stdlib.h" // IWYU pragma: keep

/*-----------------------------------------------------------*/

/*
 * Called by main when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 1 in
 * main.c.
 */
void main_blinky(void);

/*
 * Helpers
 */
void       initialize_gpio_pins(void);
TickType_t run_test();

/*-----------------------------------------------------------*/

#define TEST_DURATION_MS_DEFAULT 1300

void vTraceMonitorTask(void *pvParameters) {
  const TickType_t test_duration = (TickType_t)pvParameters;

  // Sleep for the exact duration of your test
  vTaskDelay(pdMS_TO_TICKS(test_duration));

  // The test is over. Freeze the scheduler so no more task switches occur.
  vTaskSuspendAll();
  print_trace_buffer();

  // Spin forever. The test is done.
  while (1) {
    // Hardware specific wait-for-interrupt to save power, or just empty loop
    __asm volatile("wfi");
  }
}

void main_blinky(void) {
  // Block execution until the host opens the USB serial port
  while (!stdio_usb_connected()) {
    sleep_ms(100);
  }

  printf("Starting main_blinky.\n");
  initialize_gpio_pins();

  printf("Starting test.\n");
  TickType_t test_duration = run_test();

#if !TRACE_WITH_LOGIC_ANALYZER
  // This creates the monitor task, which is responsible for printing all trace data after a certain amount of time has
  // passed. It doesn't interfere with the scheduling.
  xTaskCreate(
    vTraceMonitorTask,
    "Monitor",
    configMINIMAL_STACK_SIZE + 256, // Give it enough stack for printf
    (void *)test_duration,
    configMAX_PRIORITIES - 1,
    NULL
  );
#endif

  /* Start the tasks and timer running. */
  EDF_scheduler_start();

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
  gpio_put(mainGPIO_LED_TASK_1, 0);
  gpio_put(mainGPIO_LED_TASK_2, 0);
  gpio_put(mainGPIO_LED_TASK_3, 0);
  gpio_put(mainGPIO_LED_TASK_4, 0);
  gpio_put(mainGPIO_LED_TASK_5, 0);
  gpio_put(mainGPIO_LED_TASK_6, 0);
}

#define PASTE(x, y)        x##y
#define PASTE_EXPAND(x, y) PASTE(x, y)
TickType_t run_test() {
  // clang-format off
#if USE_EDF
  #if USE_SRP
    // If TEST_NR is 1, this becomes: return srp_test_1();
    return PASTE_EXPAND(srp_test_, TEST_NR)();
    
  #else
    // If TEST_NR is 3, this becomes: edf_test_3();
    return PASTE_EXPAND(edf_test_, TEST_NR)();
  #endif
#endif
  // clang-format on
}
