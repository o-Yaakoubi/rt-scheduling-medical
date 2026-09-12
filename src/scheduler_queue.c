/* scheduler_queue.c
 *
 * Common queue machinery shared by scheduler_fifo.c and scheduler_edf.c.
 * Both algorithms are linked into the same binary and selected at runtime
 * (--scheduler=FIFO|EDF), so the generic push/pop/locking code lives here
 * once, and only the "which job goes next" logic differs per algorithm
 * (see fifo_select_next() and edf_select_next()).
 */
#include <string.h>
#include "scheduler.h"

double timespec_diff_ms(const struct timespec *a, const struct timespec *b)
{
    double sec  = (double)(b->tv_sec  - a->tv_sec);
    double nsec = (double)(b->tv_nsec - a->tv_nsec);
    return sec * 1000.0 + nsec / 1e6;
}

void timespec_add_ms(struct timespec *ts, int ms)
{
    long total_nsec = ts->tv_nsec + (long)ms * 1000000L;
    ts->tv_sec  += total_nsec / 1000000000L;
    ts->tv_nsec  = total_nsec % 1000000000L;
}

void queue_init(job_queue_t *q, sched_policy_t policy)
{
    memset(q, 0, sizeof(*q));
    q->policy = policy;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
}

void queue_push(job_queue_t *q, job_t job)
{
    pthread_mutex_lock(&q->lock);
    if (q->count < MAX_QUEUE) {
        job.seq = q->seq_counter++;
        q->jobs[q->count++] = job;
        pthread_cond_signal(&q->cond);
    }
    /* else: queue saturated under extreme overload -> drop the reading,
     * same as a real embedded system would once its buffers are full. */
    pthread_mutex_unlock(&q->lock);
}

int queue_push_if_free(job_queue_t *q, job_t job)
{
    int pushed = 0;
    pthread_mutex_lock(&q->lock);

    int already_pending = 0;
    for (int i = 0; i < q->count; i++) {
        if (q->jobs[i].sensor_id == job.sensor_id) { already_pending = 1; break; }
    }

    if (!already_pending && q->count < MAX_QUEUE) {
        job.seq = q->seq_counter++;
        q->jobs[q->count++] = job;
        pthread_cond_signal(&q->cond);
        pushed = 1;
    }

    pthread_mutex_unlock(&q->lock);
    return pushed;
}

void queue_signal_stop(job_queue_t *q)
{
    pthread_mutex_lock(&q->lock);
    q->stop = 1;
    pthread_cond_broadcast(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

int queue_is_empty(job_queue_t *q)
{
    pthread_mutex_lock(&q->lock);
    int empty = (q->count == 0);
    pthread_mutex_unlock(&q->lock);
    return empty;
}

/* Removes and returns the job at index idx, compacting the array. */
static void remove_at(job_queue_t *q, int idx, job_t *out)
{
    *out = q->jobs[idx];
    for (int i = idx; i < q->count - 1; i++) {
        q->jobs[i] = q->jobs[i + 1];
    }
    q->count--;
}

int queue_pop_next(job_queue_t *q, job_t *out)
{
    pthread_mutex_lock(&q->lock);

    while (q->count == 0 && !q->stop) {
        pthread_cond_wait(&q->cond, &q->lock);
    }

    if (q->count == 0) { /* stopped and drained */
        pthread_mutex_unlock(&q->lock);
        return 0;
    }

    int idx = (q->policy == SCHED_POLICY_EDF) ? edf_select_next(q)
                                               : fifo_select_next(q);
    remove_at(q, idx, out);

    pthread_mutex_unlock(&q->lock);
    return 1;
}
