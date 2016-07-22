#include <assert.h>

#include "jqueue.h"


jqueue_t jqueue_new(void) {
    pool_t p;
    jqueue_t q;

    p = pool_new();
    q = (jqueue_t) pmalloco(p, sizeof(struct _jqueue_st));

    q->p = p;
    q->init_time = time(NULL);

    return q;
}

void jqueue_free(jqueue_t q) {
    assert((int) (q != NULL));

    pool_free(q->p);
}

void jqueue_push(jqueue_t q, void *data, int priority) {
    _jqueue_node_t qn, scan;

    assert((int) (q != NULL));

    q->size++;

    /* node from the cache, or make a new one */
    qn = q->cache;
    if(qn != NULL)
        q->cache = qn->next;
    else
        qn = (_jqueue_node_t) pmalloc(q->p, sizeof(struct _jqueue_node_st));

    qn->data = data;
    qn->priority = priority;

    qn->next = NULL;
    qn->prev = NULL;

    /* first one */
    if(q->back == NULL && q->front == NULL) {
        q->back = qn;
        q->front = qn;

        return;
    }

    /* find the first node with priority <= to us */
    for(scan = q->back; scan != NULL && scan->priority > priority; scan = scan->next);

    /* didn't find one, so we have top priority - push us on the front */
    if(scan == NULL) {
        qn->prev = q->front;
        qn->prev->next = qn;
        q->front = qn;

        return;
    }

    /* push us in front of scan */
    qn->next = scan;
    qn->prev = scan->prev;

    if(scan->prev != NULL)
        scan->prev->next = qn;
    else
        q->back = qn;

    scan->prev = qn;
}

void jqueue_remove(jqueue_t q, void *data) {
    assert((int) (data != NULL));
    assert((int) (q != NULL));
    _jqueue_node_t scan;
    for(scan = q->back; scan != NULL; scan = scan->next) {
        if (scan->data == data) {
            if (scan->next != NULL) {
                scan->next->prev = scan->prev;
            }
            if (scan->prev != NULL) {
                scan->prev->next = scan->next;
            }
            if (scan == q->front) {
                q->front = scan->prev;
            }
            if (scan == q->back) {
                q->back = scan->next;
            }
            q->size--;
            break;
        }
    }
}

void *jqueue_pull(jqueue_t q) {
    void *data;
    _jqueue_node_t qn;

    assert((int) (q != NULL));

    if(q->front == NULL)
        return NULL;

    data = q->front->data;

    qn = q->front;

    if(qn->prev != NULL)
        qn->prev->next = NULL;
    
    q->front = qn->prev;

    /* node to cache for later reuse */
    qn->next = q->cache;
    q->cache = qn;

    if(q->front == NULL)
        q->back = NULL;

    q->size--;

    return data;
}

int jqueue_size(jqueue_t q) {
    return q->size;
}

time_t jqueue_age(jqueue_t q) {
    return time(NULL) - q->init_time;
}
