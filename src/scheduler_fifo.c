/* scheduler_fifo.c
 *
 * First In First Out : always run whichever job arrived first, regardless
 * of how urgent its deadline is. Simple, predictable, but a critical
 * reading (e.g. ECG) can end up queued behind less critical ones.
 * Caller (queue_pop_next in scheduler_queue.c) already holds q->lock.
 */
#include "scheduler.h"

int fifo_select_next(job_queue_t *q)
{
    if (q->count == 0) return -1;

    int best = 0;
    long best_seq = q->jobs[0].seq;

    for (int i = 1; i < q->count; i++) {
        if (q->jobs[i].seq < best_seq) {
            best_seq = q->jobs[i].seq;
            best = i;
        }
    }
    return best;
}
