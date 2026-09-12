#ifndef SENSOR_H
#define SENSOR_H

#define MAX_SENSORS   20
#define MAX_NAME_LEN  32

/*
 * sensor_t - describes one medical sensor's real-time task properties.
 *
 * period_ms   : how often the sensor produces a new reading (task period)
 * deadline_ms : relative deadline for processing that reading
 * exec_ms     : simulated CPU time needed to process one reading
 * priority    : POSIX real-time priority (1-99), higher = more critical
 */
typedef struct {
    int  id;
    char name[MAX_NAME_LEN];
    int  period_ms;
    int  deadline_ms;
    int  exec_ms;
    int  priority;
} sensor_t;

/* Phase 1 - Configuration Normale (4 capteurs, ~75% CPU) */
int sensor_init_normal_set(sensor_t *sensors);

/* Phase 2 - Configuration Extreme (15 capteurs, ~200% CPU) */
int sensor_init_extreme_set(sensor_t *sensors);

/* Generates one realistic (value, alarm_flag) reading for a given sensor. */
void sensor_generate_reading(const sensor_t *s, double *out_value, int *out_alarm);

#endif /* SENSOR_H */
