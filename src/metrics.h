#ifndef METRICS_H
#define METRICS_H

#include "sensor.h"

typedef struct {
    int    sensor_id;
    char   name[MAX_NAME_LEN];
    int    period_ms;
    int    deadline_ms;
    long   exec_count;      /* jobs completed                */
    long   miss_count;      /* jobs completed after deadline */
    double latency_sum_ms;  /* sum of (start - release) times */
} sensor_metrics_t;

void metrics_init(sensor_metrics_t *m, const sensor_t *sensors, int n);
void metrics_record(sensor_metrics_t *m, int n, int sensor_id,
                     double latency_ms, int missed);
void metrics_print_report(const sensor_metrics_t *m, int n, const char *policy_name);
int  metrics_write_csv(const char *filename, const sensor_metrics_t *m, int n,
                        const char *policy_name);

#endif /* METRICS_H */
