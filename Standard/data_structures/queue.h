#ifndef QUEUE_H
#define QUEUE_H

#include "FreeRTOS_include.h" // IWYU pragma: keep

typedef struct {
  uint8_t *buffer;       // Pointer to the static memory block
  size_t   element_size; // Size of one item in bytes
  uint16_t max_elements;
  uint16_t head;
  uint16_t tail;
  uint16_t count;
} Queue_t;

void q_init(Queue_t *q, uint8_t *storage, size_t element_size, uint16_t max_elements);

bool q_empty(Queue_t *q);

bool q_enqueue(Queue_t *q, const void *item);

/**
 * @brief dequeue item if q has an element and out_item is not NULL
 */
bool q_dequeue(Queue_t *q, void *out_item);

bool q_top(Queue_t *q, void *out_item);

#endif // QUEUE_H
