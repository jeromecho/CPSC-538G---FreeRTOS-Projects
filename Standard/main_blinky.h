#ifndef MAIN_BLINKY_H
#define MAIN_BLINKY_H

// TODO: Ensure these never overlap, even with many tasks of each type.
// Something like configASSERT to verify base+count < other base.
#define mainGPIO_IDLE_TASK 0

#define mainGPIO_PERIODIC_TASK_BASE  (mainGPIO_IDLE_TASK + 1)
#define mainGPIO_PERIODIC_TASK_COUNT 3
#define mainGPIO_PERIODIC_TASK_END   (mainGPIO_PERIODIC_TASK_BASE + mainGPIO_PERIODIC_TASK_COUNT - 1)

// #define mainGPIO_APERIODIC_TASK_BASE  (mainGPIO_PERIODIC_TASK_END + 1)
#define mainGPIO_APERIODIC_TASK_BASE  (mainGPIO_PERIODIC_TASK_END + 1)
#define mainGPIO_APERIODIC_TASK_COUNT 4
#define mainGPIO_APERIODIC_TASK_END   (mainGPIO_APERIODIC_TASK_BASE + mainGPIO_APERIODIC_TASK_COUNT - 1)


#endif /* MAIN_BLINKY_H */
