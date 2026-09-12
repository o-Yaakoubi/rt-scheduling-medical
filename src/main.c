/* main.c
 *
 * Gestionnaire de Taches en Temps Reel sous Linux Embarque
 * Ordonnancement FIFO et EDF pour la Surveillance de Capteurs Medicaux
 *
 * Usage:
 *   ./medical_rtos --scheduler=FIFO|EDF --duration=SECONDS --output=file.csv [--extreme]
 *
 * Each sensor runs as its own periodic POSIX thread (a "producer") that
 * generates a reading every period_ms and pushes it as a job onto a shared
 * queue. A single "CPU" worker thread (the consumer) pulls one job at a
 * time from that queue -- in FIFO or EDF order -- and simulates processing
 * it for exec_ms. This single shared CPU is what creates real contention
 * once the combined sensor load exceeds 100%, exactly like Phase 2 in the
 * test protocol (15 sensors, ~200% CPU).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h> /* strcasecmp */

#include "sensor.h"
#include "scheduler.h"
#include "metrics.h"

typedef struct {
    const sensor_t *sensor;
    job_queue_t     *queue;
    volatile int    *stop_flag;
} producer_arg_t;

typedef struct {
    job_queue_t       *queue;
    sensor_metrics_t  *metrics;
    int                n_sensors;
} consumer_arg_t;

static void try_realtime_priority(pthread_t thread, int priority)
{
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = priority;
    /* Requires root / CAP_SYS_NICE. If it fails (no privileges), the
     * process still runs correctly -- the FIFO/EDF ordering is enforced
     * at the software queue level regardless of OS-level RT priority. */
    pthread_setschedparam(thread, SCHED_FIFO, &sp);
}

static void *producer_thread(void *arg)
{
    producer_arg_t *pa = (producer_arg_t *)arg;
    const sensor_t *s = pa->sensor;

    while (!*(pa->stop_flag)) {
        job_t job;
        memset(&job, 0, sizeof(job));

        clock_gettime(CLOCK_MONOTONIC, &job.release_time);
        job.deadline_abs = job.release_time;
        timespec_add_ms(&job.deadline_abs, s->deadline_ms);

        job.sensor_id = s->id;
        job.exec_ms   = s->exec_ms;
        sensor_generate_reading(s, &job.value, &job.alarm);

        /* single-buffered sensor: skip this sample if the previous one is
         * still waiting to be processed, instead of piling up forever */
        queue_push_if_free(pa->queue, job);

        usleep((useconds_t)s->period_ms * 1000);
    }
    return NULL;
}

/* returns 1 if finish is strictly after deadline_abs */
static int deadline_diff_positive(const struct timespec *deadline_abs, const struct timespec *finish)
{
    if (finish->tv_sec  != deadline_abs->tv_sec)
        return finish->tv_sec > deadline_abs->tv_sec;
    return finish->tv_nsec > deadline_abs->tv_nsec;
}

static void *consumer_thread(void *arg)
{
    consumer_arg_t *ca = (consumer_arg_t *)arg;
    job_t job;

    while (queue_pop_next(ca->queue, &job)) {
        struct timespec start, finish;
        clock_gettime(CLOCK_MONOTONIC, &start);

        double latency_ms = timespec_diff_ms(&job.release_time, &start);

        /* simulate the actual processing/reading cost of this task */
        usleep((useconds_t)job.exec_ms * 1000);

        clock_gettime(CLOCK_MONOTONIC, &finish);
        int missed = (deadline_diff_positive(&job.deadline_abs, &finish));

        metrics_record(ca->metrics, ca->n_sensors, job.sensor_id, latency_ms, missed);
    }
    return NULL;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s --scheduler=FIFO|EDF --duration=SECONDS --output=FILE.csv [--extreme]\n"
        "  --scheduler=FIFO|EDF  Algorithme d'ordonnancement (defaut: EDF)\n"
        "  --duration=SECONDS    Duree du test en secondes (defaut: 10)\n"
        "  --output=FILE.csv     Fichier CSV de resultats (defaut: results.csv)\n"
        "  --extreme             Utilise les 15 capteurs (Phase 2) au lieu des 4 (Phase 1)\n",
        prog);
}

int main(int argc, char **argv)
{
    sched_policy_t policy = SCHED_POLICY_EDF;
    const char *policy_name = "EDF";
    int duration_s = 10;
    char output_file[256] = "results.csv";
    int extreme = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--scheduler=", 12) == 0) {
            const char *val = argv[i] + 12;
            if (strcasecmp(val, "FIFO") == 0) {
                policy = SCHED_POLICY_FIFO;
                policy_name = "FIFO";
            } else if (strcasecmp(val, "EDF") == 0) {
                policy = SCHED_POLICY_EDF;
                policy_name = "EDF";
            } else {
                fprintf(stderr, "Scheduler inconnu: %s\n", val);
                print_usage(argv[0]);
                return 1;
            }
        } else if (strncmp(argv[i], "--duration=", 11) == 0) {
            duration_s = atoi(argv[i] + 11);
        } else if (strncmp(argv[i], "--output=", 9) == 0) {
            strncpy(output_file, argv[i] + 9, sizeof(output_file) - 1);
        } else if (strcmp(argv[i], "--extreme") == 0) {
            extreme = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Argument inconnu: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    sensor_t sensors[MAX_SENSORS];
    int n_sensors = extreme ? sensor_init_extreme_set(sensors)
                             : sensor_init_normal_set(sensors);

    printf("Gestionnaire de Taches Temps Reel - Surveillance Medicale\n");
    printf("Ordonnanceur: %-4s | Duree: %ds | Capteurs: %d (%s) | Sortie: %s\n\n",
           policy_name, duration_s, n_sensors,
           extreme ? "mode extreme" : "mode normal", output_file);

    job_queue_t queue;
    queue_init(&queue, policy);

    sensor_metrics_t metrics[MAX_SENSORS];
    metrics_init(metrics, sensors, n_sensors);

    volatile int stop_flag = 0;

    pthread_t producers[MAX_SENSORS];
    producer_arg_t producer_args[MAX_SENSORS];

    for (int i = 0; i < n_sensors; i++) {
        producer_args[i].sensor    = &sensors[i];
        producer_args[i].queue     = &queue;
        producer_args[i].stop_flag = &stop_flag;
        pthread_create(&producers[i], NULL, producer_thread, &producer_args[i]);
        try_realtime_priority(producers[i], sensors[i].priority);
    }

    pthread_t consumer;
    consumer_arg_t consumer_args = { &queue, metrics, n_sensors };
    pthread_create(&consumer, NULL, consumer_thread, &consumer_args);
    try_realtime_priority(consumer, 99); /* the CPU worker itself runs at max prio */

    sleep(duration_s);

    /* stop producers, then let the consumer drain whatever is left */
    stop_flag = 1;
    for (int i = 0; i < n_sensors; i++) pthread_join(producers[i], NULL);

    queue_signal_stop(&queue);
    pthread_join(consumer, NULL);

    metrics_print_report(metrics, n_sensors, policy_name);
    if (metrics_write_csv(output_file, metrics, n_sensors, policy_name) == 0) {
        printf("Resultats ecrits dans: %s\n", output_file);
    }

    return 0;
}
