#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <pthread.h>
#include <time.h>
#include "sensor.h"

#define MAX_QUEUE 8192

typedef enum {
    SCHED_POLICY_FIFO = 0,
    SCHED_POLICY_EDF  = 1
} sched_policy_t;

/* One "job" = one periodic reading of one sensor waiting to be processed. */
typedef struct {
    int             sensor_id;
    double          value;
    int             alarm;
    struct timespec release_time;  /* when the reading was produced   */
    struct timespec deadline_abs;  /* release_time + deadline_ms      */
    int             exec_ms;       /* simulated processing time       */
    long            seq;           /* arrival sequence number (FIFO)  */
    int             used;          /* slot occupied flag               */
} job_t;

/* Shared, thread-safe job queue. Selection policy (FIFO/EDF) decides which
 * slot queue_pop_next() removes next; both algorithms share this same
 * queue implementation and only differ in "which job is next". */
typedef struct {
    job_t           jobs[MAX_QUEUE];
    int             count;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    int             stop;
    sched_policy_t  policy;
    long            seq_counter;
} job_queue_t;

void  queue_init(job_queue_t *q, sched_policy_t policy);
void  queue_push(job_queue_t *q, job_t job);
/* Pushes only if this sensor has no job already waiting in the queue.
 * Returns 1 if pushed, 0 if dropped (previous sample still unprocessed).
 * This models a real single-buffered sensor: it won't stack up stale
 * readings forever, it just drops a sample if the CPU can't keep up. */
int   queue_push_if_free(job_queue_t *q, job_t job);
int   queue_pop_next(job_queue_t *q, job_t *out); /* blocks; returns 0 if stopped & empty */
void  queue_signal_stop(job_queue_t *q);
int   queue_is_empty(job_queue_t *q);

/* Implemented differently in scheduler_fifo.c and scheduler_edf.c.
 * Both return the index of the job to run next among q->jobs[0..count-1],
 * or -1 if the queue is empty. Caller already holds q->lock. */
int fifo_select_next(job_queue_t *q);
int edf_select_next(job_queue_t *q);

/* Utility: (b - a) in milliseconds */
double timespec_diff_ms(const struct timespec *a, const struct timespec *b);
void   timespec_add_ms(struct timespec *ts, int ms);

#endif /* SCHEDULER_H */
