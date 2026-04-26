# SRP - Future

## Binary semaphore implementation
The implementation of semaphore acquisition is based on critical sections. This could easily be updated to use the FreeRTOS- (or native pico-) semaphores, which might make more sense if this project were to be supported going forward. 