#include <lib/fifosize.h>
#include <mm/alloc.h>


queueSize* queueSize_create(uint32_t capacity) {
    queueSize* fifo_buffer = (queueSize*)kmalloc(sizeof(queueSize));
    if (fifo_buffer) {
        fifo_buffer->buffer = (uint8_t*)kmalloc(capacity);
        if (fifo_buffer->buffer) {
            fifo_buffer->capacity = capacity;
            fifo_buffer->size = 0;
            fifo_buffer->front = 0;
            fifo_buffer->rear = 0;
            return fifo_buffer;
        } else {
            kfree(fifo_buffer);
            return NULL;
        }
    }
    return NULL;
}

void queueSize_destroy(queueSize* fifo_buffer) {
    if (fifo_buffer) {
        kfree(fifo_buffer->buffer);
        kfree(fifo_buffer);
    }
}

void queueSize_clear(queueSize* fifo_buffer) {
    if (fifo_buffer) {
        fifo_buffer->size = 0;
        fifo_buffer->front = 0;
        fifo_buffer->rear = 0;
    }
}

bool queueSize_is_empty(queueSize* fifo_buffer) {
    return (fifo_buffer && fifo_buffer->size == 0) ? true : false;
}

uint32_t queueSize_get_size(queueSize* fifo_buffer) {
    return (fifo_buffer) ? fifo_buffer->size : 0;
}

uint32_t queueSize_get_capacity(queueSize* fifo_buffer) {
    return (fifo_buffer) ? fifo_buffer->capacity : 0;
}

uint32_t queueSize_get_free(queueSize* fifo_buffer) {
    if (fifo_buffer) {
        return fifo_buffer->capacity - fifo_buffer->size;
    }
    return 0;
}

uint32_t queueSize_enqueue(queueSize* fifo_buffer, uint8_t* data, uint32_t size) {
    if (fifo_buffer && data && size > 0) {
        if (size > queueSize_get_free(fifo_buffer)) {
            return -1; // Not enough space
        }
        for (uint32_t i = 0; i < size; i++) {
            fifo_buffer->buffer[fifo_buffer->rear] = data[i];
            fifo_buffer->rear = (fifo_buffer->rear + 1) % fifo_buffer->capacity;
            fifo_buffer->size++;
        }
        return size;
    }
    return -1;
}

uint32_t queueSize_dequeue(queueSize* fifo_buffer, uint8_t* data, uint32_t size) {
    if (fifo_buffer && data && size > 0 && size <= fifo_buffer->size) {
        for (uint32_t i = 0; i < size; i++) {
            data[i] = fifo_buffer->buffer[fifo_buffer->front];
            fifo_buffer->front = (fifo_buffer->front + 1) % fifo_buffer->capacity;
            fifo_buffer->size--;
        }
        return size;
    }
    return -1;
}
