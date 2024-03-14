/*
 * FIFO(First In First Out) buffer with queue size 
 *
 * Used by UNIX sockets for example, and i don't really know how to do it with clasic queue's so here we are
 *
 * Okay, actually my idea with classic queue for UNIX sockets are failed, so i added this
 * This component is required for kernel components that required the queue and size of data that is added to it
*/
#ifndef FIFOSIZE_H
#define FIFOSIZE_H

#include <typedefs.h>

typedef struct {
    uint8_t* buffer;
    uint32_t capacity;
    uint32_t size;
    uint32_t front;
    uint32_t rear;
} queueSize; // Queue with size and limits

queueSize* queueSize_create(uint32_t capacity);
void queueSize_clear(queueSize* fifo_buffer);
bool queueSize_is_empty(queueSize* fifo_buffer);
uint32_t queueSize_get_size(queueSize* fifo_buffer);
uint32_t queueSize_get_capacity(queueSize* fifo_buffer);
uint32_t queueSize_get_free(queueSize* fifo_buffer);
uint32_t queueSize_enqueue(queueSize* fifo_buffer, uint8_t* data, uint32_t size);
uint32_t queueSize_dequeue(queueSize* fifo_buffer, uint8_t* data, uint32_t size);
void queueSize_destroy(queueSize* fifo_buffer);

#endif
