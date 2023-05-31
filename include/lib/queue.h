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

static inline void queue_remove(queue_t *q,void *value) {
    if (!q || !q->size) return;
    for (struct queue_node *n = q->head; n; n = n->next) {
        if (n->value == value) {
            if (n->prev) {
                dequeue(q);
            } else if (!n->next) {
                --q->size;
                q->tail = q->tail->prev;
                q->tail->next = NULL;
                kfree(n);
            } else {
                --q->size;
                n->prev->next = n->next;
                n->next->prev = n->prev;
                kfree(n);
            }
            break;
        }
    }
}

#endif
