/* scheduler_edf.c
 *
 * Earliest Deadline First : always run the job whose absolute deadline is
 * closest, like hospital triage. Optimal for uniprocessor scheduling
 * (Liu & Layland, 1973) when the system is schedulable.
 * Caller (queue_pop_next in scheduler_queue.c) already holds q->lock.
 */
#include "scheduler.h"

/* returns <0 if a before b, 0 if equal, >0 if a after b */
static int deadline_cmp(const struct timespec *a, const struct timespec *b)
{
    if (a->tv_sec  != b->tv_sec)  return (a->tv_sec  < b->tv_sec)  ? -1 : 1;
    if (a->tv_nsec != b->tv_nsec) return (a->tv_nsec < b->tv_nsec) ? -1 : 1;
    return 0;
}

int edf_select_next(job_queue_t *q)
{
    if (q->count == 0) return -1;

    int best = 0;
    for (int i = 1; i < q->count; i++) {
        if (deadline_cmp(&q->jobs[i].deadline_abs, &q->jobs[best].deadline_abs) < 0) {
            best = i;
        }
    }
    return best;
}
