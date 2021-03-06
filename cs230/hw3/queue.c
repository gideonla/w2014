#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "locks.h"


struct l_queue *create_queues(int n_queues, int size, char *lock_type, void *lock_info) {
    struct l_queue *q = calloc (n_queues, sizeof(struct l_queue));
    for (int i = 0; i < n_queues; i++) {
        q[i].items = calloc (size, sizeof(void *));
        q[i].length = (unsigned) size;
        if (lock_type == NULL) 
            q[i].lock = create_lock("noop", NULL);
        else
            q[i].lock = create_lock(lock_type, lock_info);
        q[i].queue_id = i;
    }
    return q;
}

void destroy_queues(int n_queues, struct l_queue *q) {
    for (int i = 0; i < n_queues; i++) {
        q[i].lock->destroy_lock (q[i].lock);
        free (q[i].items);
    }
    free (q);
}

int enq(struct l_queue *q, void *obj) {
    int head = q->head;
    int tail = q->tail;
    int len = q->length;

    if (tail - head == len) {
        return 1;
    }

    q->items[tail & (len - 1)] = obj;
    q->n_enqueues += 1;
    q->tail = tail + 1;
    return 0;
}

int deq(struct l_queue *q, void **obj_ptr) {
    int head = q->head;
    int tail = q->tail;
    int len = q->length;

    if (tail == head) {
#ifdef TESTING1
        printf("%d empty\n", q->queue_id);
#endif
        return 1;
    }

#ifdef TESTING1
    if (tail - head == len) {
        printf("%d started\n", q->queue_id);
    }
#endif

    *obj_ptr = q->items[head & (len - 1)];
    q->head = head + 1;
    return 0;
}

int check_free (struct l_queue *q) {
    int head = q->head;
    int tail = q->tail;
    int len = q->length;
  
    if (tail - head == len) {
        return 0;
    }
    else {
        return 1;
    }
}

int get_n_enqueues (struct l_queue *q) {
    return (q->n_enqueues);
}
