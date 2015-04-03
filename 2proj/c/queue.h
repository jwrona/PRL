#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>

typedef struct {
    unsigned char *data;
    size_t size, head, tail;
} queue_t;

queue_t *queue_init(const size_t elements)
{
    queue_t *q;

    q = calloc(1, sizeof(queue_t));
    if (q == NULL) {
	return NULL;
    }

    q->size = elements + 1;
    q->data = malloc(q->size * sizeof(unsigned char));
    if (q->data == NULL) {
	free(q);
	return NULL;
    }

    return q;
}

void queue_destroy(queue_t *q)
{
    if (q != NULL) {
	free(q->data);
    }
    free(q);
}

unsigned queue_empty(const queue_t *q)
{
    return (q->head == q->tail);
}

unsigned queue_full(const queue_t *q)
{
    return (q->head == ((q->tail + (q->size - 1)) % q->size));
}

void queue_enqueue(queue_t *q, unsigned char new_elem)
{
    if (queue_full(q)) {
        return; /* queue full, enqueue nothing */
    }

    q->data[q->head] = new_elem;
    q->head = (q->head + 1) % q->size;
}

unsigned char queue_dequeue(queue_t *q)
{
    unsigned char ret;

    if (queue_empty(q)) {
        return 0; /* queue empty, dequeue nothing */
    }

    ret = q->data[q->tail];
    q->tail = (q->tail + 1) % q->size;

    return ret;
}

unsigned char queue_front(queue_t *q)
{
    if (queue_empty(q)) {
        return 0; /* queue empty */
    }

    return q->data[q->tail];
}

#endif /* QUEUE_H */
