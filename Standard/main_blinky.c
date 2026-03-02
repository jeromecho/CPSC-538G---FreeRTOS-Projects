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

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Library includes. */
#include <stdio.h>
#include "hardware/gpio.h"

// EDF Scheduler includes
#include "edf_scheduler.h"

// Other includes
#include "main_blinky.h"
#include "pico/stdlib.h"

/* Priorities at which the tasks are created. */
#define mainQUEUE_RECEIVE_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define mainQUEUE_SEND_TASK_PRIORITY    (tskIDLE_PRIORITY + 1)

/* The rate at which data is sent to the queue.  The 200ms value is converted
to ticks using the portTICK_PERIOD_MS constant. */
#define mainQUEUE_SEND_FREQUENCY_MS (200 / portTICK_PERIOD_MS)

/* The number of items the queue can hold.  This is 1 as the receive task
will remove items as they are added, meaning the send task should always find
the queue empty. */
#define mainQUEUE_LENGTH (1)

/* The LED toggled by the Rx task. */
#define mainTASK_LED (PICO_DEFAULT_LED_PIN)

/*-----------------------------------------------------------*/

/*
 * Called by main when mainCREATE_SIMPLE_BLINKY_DEMO_ONLY is set to 1 in
 * main.c.
 */
void main_blinky(void);

/*
 * The tasks as described in the comments at the top of this file.
 */
static void vPeriodicTask(void *pvParameters);
static void vMainLEDBlinkTask(void *pvParameters);

/*
 * Helpers
 */
void initialize_gpio_pins(void);

/*-----------------------------------------------------------*/

/* The queue used by both tasks. */
static QueueHandle_t xQueue = NULL;

/*-----------------------------------------------------------*/
// TODO: Refactor testing logic to a separate file
void vTestRunner9(void *pvParameters) {
  // --- TEST A: Admissible Drop-in ---
  // Base Task: 160ms work, 800ms period
  xTaskCreatePeriodic(
    vPeriodicTask, "Base_A", configMINIMAL_STACK_SIZE, (void *)(8 * 20), pdMS_TO_TICKS(8 * 100),
    pdMS_TO_TICKS(8 * 100), NULL
  );

  // Wait 5 cycles (500ms) to show stable execution
  vTaskDelay(pdMS_TO_TICKS(500));

  // Drop-in Task: 400ms work, 800ms period.
  // Total Demand: 560ms < 800ms. Should pass PDC.
  // TODO: show why total demand is not exceeded
  xTaskCreatePeriodic(
    vPeriodicTask, "Drop_A", configMINIMAL_STACK_SIZE, (void *)(8 * 50), pdMS_TO_TICKS(8 * 100),
    pdMS_TO_TICKS(8 * 100), NULL
  );
  vTaskDelete(NULL);
}

void vTestRunner10(void *pvParameters) {
  // --- TEST B: Inadmissible Drop-in ---
  // Base Task: 20ms work, 100ms period (U=0.2)
  xTaskCreatePeriodic(
    vPeriodicTask, "Base_B", configMINIMAL_STACK_SIZE, (void *)20, pdMS_TO_TICKS(100),
    pdMS_TO_TICKS(100), NULL
  );

  vTaskDelay(pdMS_TO_TICKS(500));

  // Drop-in Task: 90ms work, 200ms period (U=0.45)
  // At L=100, Demand = 20 + 90 = 110ms. PDC Violation!
  BaseType_t result = xTaskCreatePeriodic(
    vPeriodicTask, "Drop_B", configMINIMAL_STACK_SIZE, (void *)90, pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(100), NULL
  );
  vTaskDelete(NULL);
}

/*-----------------------------------------------------------*/

void main_blinky(void) {
  // Block execution until the host opens the USB serial port
  while (!stdio_usb_connected()) {
    sleep_ms(100);
  }

  printf("Starting main_blinky.\n");
  initialize_gpio_pins();

  /**
   * Tests for Missed Deadline (Note: Turn off/Comment Out Admission Control to Allow Easy Deadline
   * Miss)
   */
  // TEST 11: Missed Deadline (Total Utilization: 105%)
  xTaskCreatePeriodic(
    vPeriodicTask, "Task_A", 512, (void *)40, pdMS_TO_TICKS(100), pdMS_TO_TICKS(100), NULL
  );
  xTaskCreatePeriodic(
    vPeriodicTask, "Task_B", 512, (void *)130, pdMS_TO_TICKS(200), pdMS_TO_TICKS(200), NULL
  );

  /**
   * Tests for Drop-in of Tasks while System is Running
   */
  // TODO: Not sure if vTaskCreate calling xTaskCreatePeriodic, which calls vTaskCreate is a
  //       good design
  // TODO: magic numbers for priority of below function calls
  // TEST 10: Inadmissible Drop-in
  /*
   */
  /*
  xTaskCreate(vTestRunner10, "test runner 10", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

  // TEST 9: Admissible Drop-in
  xTaskCreate(vTestRunner9, "test runner 9", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
  */

  /**
   * Admission Control Tests
   */
  /*
   */
  /*
  // TEST 8: BARELY NON-ADMISSIBLE BY DEMAND (U is only 42% but demand > 1 at L = 50)
  xTaskCreatePeriodic(
    vPeriodicTask, "Fail_A", configMINIMAL_STACK_SIZE, (void *)pdMS_TO_TICKS(11), pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(50), NULL
  );
  xTaskCreatePeriodic(
    vPeriodicTask, "Fail_B", configMINIMAL_STACK_SIZE, (void *)pdMS_TO_TICKS(40),
    pdMS_TO_TICKS(200), pdMS_TO_TICKS(50), NULL
  );

  // TEST 7: BARELY ADMISSIBLE BY PROCESSOR DEMAND (both U and demand are below upper bounds)
  xTaskCreatePeriodic(
    vPeriodicTask, "Adm_A", configMINIMAL_STACK_SIZE, (void *)pdMS_TO_TICKS(10), pdMS_TO_TICKS(50),
    pdMS_TO_TICKS(50), NULL
  );
  xTaskCreatePeriodic(
    vPeriodicTask, "Adm_B", configMINIMAL_STACK_SIZE, (void *)pdMS_TO_TICKS(40), pdMS_TO_TICKS(200),
    pdMS_TO_TICKS(50), NULL
  );

  // TEST 6: BARELY NON-ADMISSIBLE BY UTILIZATION (10 tasks * 11ms = 110ms demand every 100ms)
  // Total Utilization = 1.1 (110%)
  for (int i = 0; i < 10; i++) {
    char taskName[16];
    sprintf(taskName, "Fail_%d", i);
    xTaskCreatePeriodic(
      vPeriodicTask, taskName, configMINIMAL_STACK_SIZE,
      (void *)pdMS_TO_TICKS(11), // C: 11ms
      pdMS_TO_TICKS(100),        // T: 100ms
      pdMS_TO_TICKS(100),        // D: 100ms
      NULL
    );
  }

  // TEST 5: BARELY ADMISSIBLE BY UTILIZATION (10 tasks * 10ms = 100ms demand every 100ms)
  // Total Utilization = 1.0 (100%)
  for (int i = 0; i < 10; i++) {
    char taskName[16];
    sprintf(taskName, "Adm_%d", i);
    xTaskCreatePeriodic(
      vPeriodicTask, taskName, configMINIMAL_STACK_SIZE,
      (void *)pdMS_TO_TICKS(10), // C: 10ms
      pdMS_TO_TICKS(100),        // T: 100ms
      pdMS_TO_TICKS(100),        // D: 100ms
      NULL
    );
  }

  // TEST4: 100 Tasks ADMISSIBLE
  for (int i = 0; i < 100; i++) {
    // NB: This breaks without downstream copying of task
    char taskName[16];
    sprintf(taskName, "test %d", i);
    xTaskCreatePeriodic(
      vPeriodicTask, taskName, configMINIMAL_STACK_SIZE,
      (void *)pdMS_TO_TICKS(8), // C: 8ms
      pdMS_TO_TICKS(1000),      // T: 1000ms
      pdMS_TO_TICKS(1000),      // D: 1000ms
      NULL
    );
  }
   */

  // TEST3: 100 Tasks NON-ADMISSIBLE
  /*
  for (int i = 0; i < 100; i++) {
    char taskName[16];
    sprintf(taskName, "test %d", i);
    xTaskCreatePeriodic(
      vPeriodicTask, taskName, configMINIMAL_STACK_SIZE,
      (void *)pdMS_TO_TICKS(15), // C: 15ms
      pdMS_TO_TICKS(1000),       // T: 1000ms
      pdMS_TO_TICKS(500),        // D: 500ms (Tight!)
      NULL
    );
  }
  */

  /**
   * Tests for Base Functionality
   */

  /*
  // Test 2: Mark's Deadline DNE Period Smoke Test
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 1",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(200), // Completion time
    pdMS_TO_TICKS(600),         // Period
    pdMS_TO_TICKS(400),         // Relative Deadline
    NULL                        // Task handle
  );
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 2",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(200), // Completion time
    pdMS_TO_TICKS(800),         // Period
    pdMS_TO_TICKS(500),         // Relative Deadline
    NULL                        // Task handle
  );
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 3",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(300), // Completion time
    pdMS_TO_TICKS(900),         // Period
    pdMS_TO_TICKS(700),         // Relative Deadline
    NULL                        // Task handle
  );
  */

  /*
  // Smoke Test for Periodic Tasks (relative deadline == period)

  // Test 1: Periodic Task 1
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 1",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(200), // Completion time
    pdMS_TO_TICKS(600),         // Period
    pdMS_TO_TICKS(600),         // Relative Deadline
    NULL                        // Task handle
  );

  // Periodic Task 2
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 2",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(100), // Completion time
    pdMS_TO_TICKS(200),         // Period
    pdMS_TO_TICKS(200),         // Relative Deadline
    NULL                        // Task handle
  );
  */

  /* Start the tasks and timer running. */
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
/*-----------------------------------------------------------*/
static void vPeriodicTask(void *pvParameters) {
  // TODO: Replace with macro, so that the scheduler can be responsible for marking tasks as done
  // and suspending them, instead of the tasks themselves.
  // TODO: This would also mean that the scheduler can be responsible for deleting aperiodic tasks
  // once they are finished executing.
  const BaseType_t xCompletionTime = (BaseType_t)pvParameters;
  TickType_t       previousTick    = xTaskGetTickCount();

  BaseType_t xTimeSlicesExecutedThusFar = 0;

  for (;;) {
    TickType_t currentTick = xTaskGetTickCount();
    if (currentTick != previousTick) {
      previousTick = currentTick;
      xTimeSlicesExecutedThusFar++;
    }
    if (xTimeSlicesExecutedThusFar == xCompletionTime) {
      xTimeSlicesExecutedThusFar = 0;
      taskDone(xTaskGetCurrentTaskHandle());
      vTaskSuspend(NULL);
    }
    // vTaskDelay(pdMS_TO_TICKS(200));
  }
}

static void vMainLEDBlinkTask(void *pvParameters) {
  const TickType_t xDelay250ms = pdMS_TO_TICKS(250UL);

  /* Remove compiler warning about unused parameter. */
  (void)pvParameters;

  for (;;) {
    /* Toggle the LED each time data is received. */
    gpio_xor_mask(1 << mainTASK_LED);

    /* Wait for the next cycle. */
    vTaskDelay(xDelay250ms);
  }
}
/*-----------------------------------------------------------*/

void initialize_gpio_pins(void) {
  gpio_put(mainTASK_LED, 0);
  gpio_put(mainGPIO_LED_TASK_1, 0);
  gpio_put(mainGPIO_LED_TASK_2, 0);
  gpio_put(mainGPIO_LED_TASK_3, 0);
  gpio_put(mainGPIO_LED_TASK_4, 0);
  gpio_put(mainGPIO_LED_TASK_5, 0);
  gpio_put(mainGPIO_LED_TASK_6, 0);
}
