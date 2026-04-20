#include "queue.h"

#include "task.h"

#include <string.h>

void q_init(Queue_t *q, uint8_t *storage, size_t element_size, uint16_t max_elements) {
  q->buffer       = storage;
  q->element_size = element_size;
  q->max_elements = max_elements;
  q->head         = 0;
  q->tail         = 0;
  q->count        = 0;
}

bool q_empty(Queue_t *q) { return q->count == 0; }

// NB: critical sections below may be optional as long we guarantee no concurrency-related
//     issues with accessing the queue
bool q_enqueue(Queue_t *q, const void *item) {
  bool success = false;
  taskENTER_CRITICAL();

  if (q->count < q->max_elements) {
    // Calculate destination address and copy bytes
    uint8_t *dest = q->buffer + (q->tail * q->element_size);
    memcpy(dest, item, q->element_size);

    q->tail = (q->tail + 1) % q->max_elements;
    q->count++;
    success = true;
  }
  taskEXIT_CRITICAL();
  return success;
}

bool q_dequeue(Queue_t *q, void *out_item) {
  bool success = false;
  taskENTER_CRITICAL();
  if (q->count > 0) {
    // Calculate source address and copy bytes
    uint8_t *src = q->buffer + (q->head * q->element_size);
    if (out_item != NULL) {
      memcpy(out_item, src, q->element_size);
    }
    q->head = (q->head + 1) % q->max_elements;
    q->count--;
    success = true;
  }
  taskEXIT_CRITICAL();
  return success;
}

bool q_top(Queue_t *q, void *out_item) {
  bool success = false;
  taskENTER_CRITICAL();
  if (q->count > 0) {
    // Calculate source address and copy bytes
    uint8_t *src = q->buffer + (q->head * q->element_size);
    memcpy(out_item, src, q->element_size);
    success = true;
  }
  taskEXIT_CRITICAL();
  return success;
}
