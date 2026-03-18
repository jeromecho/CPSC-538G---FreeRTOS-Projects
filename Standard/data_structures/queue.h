#ifndef QUEUE_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  uint8_t *buffer;       // Pointer to the static memory block
  size_t   element_size; // Size of one item in bytes
  uint16_t max_elements;
  uint16_t head;
  uint16_t tail;
  uint16_t count;
} Queue_t;

#endif // QUEUE_H