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

/*-----------------------------------------------------------*/

/* The queue used by both tasks. */
static QueueHandle_t xQueue = NULL;

/*-----------------------------------------------------------*/

void main_blinky(void) {
  printf(" Starting main_blinky.\n");

  gpio_put(mainTASK_LED, 1);
  gpio_put(mainGPIO_LED_TASK_1, 1);
  gpio_put(mainGPIO_LED_TASK_2, 1);
  gpio_put(mainGPIO_LED_TASK_3, 1);
  gpio_put(mainGPIO_LED_TASK_4, 1);

  // Periodic Task 1
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 1",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(200), // Completion time
    pdMS_TO_TICKS(600),         // Period
    NULL                        // Task handle
  );

  // Periodic Task 2
  xTaskCreatePeriodic(
    vPeriodicTask,              // Task function
    "Periodic Task 2",          // Task name
    configMINIMAL_STACK_SIZE,   // Stack depth
    (void *)pdMS_TO_TICKS(100), // Completion time
    pdMS_TO_TICKS(200),         // Period
    NULL                        // Task handle
  );

  // xTaskCreate(
  //   vMainLEDBlinkTask, /* The function that implements the task. */
  //   "MainBlink", /* The text name assigned to the task - for debug only as it is not used by the
  //                   kernel. */
  //   configMINIMAL_STACK_SIZE,        /* The size of the stack to allocate to the task. */
  //   NULL,                            /* The parameter passed to the task - not used in this case.
  //   */ mainQUEUE_RECEIVE_TASK_PRIORITY, /* The priority assigned to the task. */ NULL
  // );                                 /* The task handle is not required, so NULL is passed. */

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
  TickType_t previousTick = xTaskGetTickCount();

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

// void traceENTER_vTaskSuspend(void) {
void task_switched_out(void) {
  // A task's GPIO pin needs to be set low when it is suspended
  // Should we use pxCurrentTCB here instead?
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  TaskHandle_t idle_task    = xTaskGetIdleTaskHandle();

  // if (current_task == idle_task) {
  //   gpio_put(mainGPIO_LED_TASK_3, 1);
  //   return;
  // }

  // // TODO: This is a bit hacky, but it works for demonstration purposes.  A more robust solution
  // // would be to have the scheduler set the GPIO pin for a task when it changes the task's state.
  // if (current_task == periodic_tasks[0].tmb.handle) {
  //   gpio_put(mainGPIO_LED_TASK_1, 1);
  // } else if (current_task == periodic_tasks[1].tmb.handle) {
  //   gpio_put(mainGPIO_LED_TASK_2, 1);
  // }
}

// void traceENTER_vTaskResume(void) {
void task_switched_in(void) {
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  TaskHandle_t idle_task    = xTaskGetIdleTaskHandle();
  int          number_of_tasks = uxTaskGetNumberOfTasks();

  // TaskStatus_t xTaskDetails;
  // vTaskGetInfo( /* The handle of the task being queried. */
  //                 current_task,
  //                 /* The TaskStatus_t structure to complete with information
  //                 on xTask. */
  //                 &xTaskDetails,
  //                 /* Include the stack high water mark value in the
  //                 TaskStatus_t structure. */
  //                 pdTRUE,
  //                 /* Include the task state in the TaskStatus_t structure. */
  //                 eInvalid );
  // printf("Current task name: %s\n", xTaskDetails.pcTaskName);

  if (current_task == idle_task) {
    gpio_xor_mask(1 << mainGPIO_LED_TASK_3);
  } else if (current_task == periodic_tasks[0].tmb.handle) {
    gpio_xor_mask(1 << mainGPIO_LED_TASK_1);
  } else if (current_task == periodic_tasks[1].tmb.handle) {
    gpio_xor_mask(1 << mainGPIO_LED_TASK_2);
  } else {
    gpio_xor_mask(1 << mainGPIO_LED_TASK_4);
  }


  if (current_task == NULL) {
    // printf("Current task is NULL");
    return;
  }

  // if (current_task == idle_task) {
  //   gpio_put(mainGPIO_LED_TASK_3, 0);
  //   return;
  // }

  // Get the TMB associated with the current task
  // printf("Num Periodic Tasks: %d, Num FreeRTOS Tasks: %d\n",
  // periodic_task_count,number_of_tasks);
  // TMB_Periodic_t *current_tmb = NULL;
  // int             tmb_index   = -1;
  // for (int i = 0; i < periodic_task_count; i++) {
  //   if (current_task == periodic_tasks[i].tmb.handle) {
  //     current_tmb = &periodic_tasks[i];
  //     tmb_index   = i;
  //     break;
  //   }
  // }
  // if (current_tmb == NULL) {
  //   printf("No periodic tasks are associated with the current task being switched in\n");
  // }
  // printf("Task %d is done: %d\n", tmb_index, current_tmb->is_done);

  // A task's GPIO pin needs to be set high when it is resumed
  // TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  // if (current_task == periodic_tasks[0].tmb.handle) {
  //   gpio_put(mainGPIO_LED_TASK_1, 0);
  // } else if (current_task == periodic_tasks[1].tmb.handle) {
  //   gpio_put(mainGPIO_LED_TASK_2, 0);
  // }
}

void vApplicationTickHook(void) {
  // gpio_xor_mask(1 << mainGPIO_LED_TASK_4);

  setSchedulable();
  updatePriorities();
}
