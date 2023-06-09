#ifndef QUEUE_H
#define QUEUE_H

#include <mm/alloc.h>

// FIFO type queue for processes or other thinks like signals
struct queue_node {
    void *value;
    struct queue_node *prev;
    struct queue_node *next;
};

typedef struct queue {
    int size;
    struct queue_node *head;
    struct queue_node *tail;
} queue_t;

#define queue_for(n, q) for (struct queue_node *(n) = (q)->head; (n); (n) = (n)->next)

static inline queue_t *queue_new() {
    queue_t *q = kmalloc(sizeof(queue_t));
    if (!q) return NULL;
    memset(q,0,sizeof(queue_t));
    return q;
}
static inline struct queue_node *enqueue(queue_t *q,void *element) {
    if (!q) return NULL;
    struct queue_node *node = kmalloc(sizeof(struct queue_node));
    if (!node) return NULL;
    memset(node,0,sizeof(struct queue_node));
    node->value = element;
    if (!q->size) {
        q->head = q->tail = node;
    } else {
        node->prev = q->tail;
        q->tail->next = node;
        q->tail = node;
    }
    ++q->size;
    return node;
}

static inline void *dequeue(queue_t *q) {
    if (!q || !q->size) return NULL;
    --q->size;
    struct queue_node *head = q->head;
    q->head = head->next;
    if (q->head) {
        q->head->prev = NULL;
    }
    if (head == q->tail) {
        q->tail = NULL;
    }
    void *value = head->value;
    kfree(head);
    return value;
}
// Copy paste is always works
static inline void queue_remove(queue_t *queue,void *value) {
    if (!queue || !queue->size) return;
     queue_for (node, queue) {
        if (node->value == value) {
            if (!node->prev) {    /* Head */
                dequeue(queue);
            } else if (!node->next) {   /* Tail */
                --queue->size;
                queue->tail = queue->tail->prev;
                queue->tail->next = NULL;
                kfree(node);
            } else {
                --queue->size;
                node->prev->next = node->next;
                node->next->prev = node->prev;
                kfree(node);
            }

            break;
        }
    }
}

#endif
