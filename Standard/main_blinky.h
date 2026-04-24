#ifndef MAIN_BLINKY_H
#define MAIN_BLINKY_H

#define mainGPIO_STATE_HIGH 1
#define mainGPIO_STATE_LOW  0

// TODO: Ensure these never overlap, even with many tasks of each type.
// Something like configASSERT to verify base+count < other base.
#define mainGPIO_IDLE_TASK_0 0
#define mainGPIO_IDLE_TASK_1 1

#define mainGPIO_UID_TASK_BASE  (mainGPIO_IDLE_TASK_1 + 1)
#define mainGPIO_UID_TASK_COUNT 7
#define mainGPIO_UID_TASK_END   (mainGPIO_UID_TASK_BASE + mainGPIO_UID_TASK_COUNT - 1)


#endif /* MAIN_BLINKY_H */
