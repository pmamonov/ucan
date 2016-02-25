#include <stdio.h>
#include <string.h>
#include "uqueue.h"

static int bug_report = 2;

static int queue_lock(struct queue *q)
{
	int ret = -1;

	taskDISABLE_INTERRUPTS();
	if (q->sem == 0) {
		q->sem = 1;
		ret = 0;
	}
	taskENABLE_INTERRUPTS();
	return ret;
}

static void queue_unlock(struct queue *q)
{
	taskDISABLE_INTERRUPTS();
		if (!q->sem)
			printf("BUG: %s: sem = %d\n\r", __func__, q->sem);
		q->sem = 0;
	taskENABLE_INTERRUPTS();
}

void queue_init(struct queue *q, int sz, int cap, void *ptr)
{
	q->sz = sz;
	q->cap = cap;
	q->ptr = ptr;
	q->len = 0;
	q->in = 0;
	q->out = 0;
	q->sem = 0;
}

static inline void queue_check(struct queue *q, char *func)
{
	if (bug_report &&
	    q->len != (q->in >= q->out ? q->in - q->out : q->in + q->cap - q->out)) {
		printf("BUG: %s: in=%d out=%d len=%d q=%p\n\r",
			func,
			q->in,
			q->out,
			q->len,
			q);
		bug_report--;
	}
}

int queue_push(struct queue *q, void *ptr)
{
	if (queue_lock(q))
		return -1;
	if (q->len >= q->cap) {
		queue_unlock(q);
		return -1;
	}
	queue_check(q, "push-in");
	memcpy(q->ptr + q->sz * q->in, ptr, q->sz);
	q->in = (q->in + 1) % q->cap;
	q->len += 1;
	queue_check(q, "push-out");
	queue_unlock(q);
	return 0;
}

int queue_pop(struct queue *q, void *ptr)
{
	if (queue_lock(q))
		return -1;
	if (q->len < 1) {
		queue_unlock(q);
		return -1;
	}
	queue_check(q, "pop-in");
	memcpy(ptr, q->ptr + q->sz * q->out, q->sz);
	q->out = (q->out + 1) % q->cap;
	q->len -= 1;
	queue_check(q, "pop-out");
	queue_unlock(q);
	return 0;
}


int queue_swap(struct queue *q1, struct queue *q2)
{
	struct queue tmp;

	taskDISABLE_INTERRUPTS();

	if (q1->sem || q2->sem) {
		taskENABLE_INTERRUPTS();
		return -1;
	}
	queue_check(q1, "swap-in-q1");
	queue_check(q2, "swap-in-q2");

	memcpy(&tmp, q1, sizeof(*q1));
	memcpy(q1, q2, sizeof(*q1));
	memcpy(q2, &tmp, sizeof(*q1));

	queue_check(q1, "swap-out-q1");
	queue_check(q2, "swap-out-q2");
	taskENABLE_INTERRUPTS();
	return 0;
}
