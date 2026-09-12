#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sensor.h"

/* ---- Phase 1 : 4 capteurs, mode normal ---------------------------------- */
int sensor_init_normal_set(sensor_t *sensors)
{
    int n = 0;

    strcpy(sensors[n].name, "ECG");
    sensors[n].id = n; sensors[n].period_ms = 20; sensors[n].deadline_ms = 20;
    sensors[n].exec_ms = 2; sensors[n].priority = 90; n++;

    strcpy(sensors[n].name, "SpO2");
    sensors[n].id = n; sensors[n].period_ms = 50; sensors[n].deadline_ms = 50;
    sensors[n].exec_ms = 5; sensors[n].priority = 80; n++;

    strcpy(sensors[n].name, "Pression");
    sensors[n].id = n; sensors[n].period_ms = 100; sensors[n].deadline_ms = 100;
    sensors[n].exec_ms = 10; sensors[n].priority = 70; n++;

    strcpy(sensors[n].name, "Temperature");
    sensors[n].id = n; sensors[n].period_ms = 200; sensors[n].deadline_ms = 200;
    sensors[n].exec_ms = 15; sensors[n].priority = 50; n++;

    return n;
}

/* ---- Phase 2 : 15 capteurs, mode extreme (charge CPU ~200%) ------------- */
int sensor_init_extreme_set(sensor_t *sensors)
{
    /* name, period_ms, deadline_ms, exec_ms — taken from the test protocol */
    static const struct { const char *name; int period; int deadline; int exec; } cfg[] = {
        {"ECG_HeartRate",     20,  20,  2},
        {"ECG_2",             20,  20,  2},
        {"ECG_3",             20,  20,  2},
        {"SpO2_Oxygen",       50,  50,  5},
        {"SpO2_2",            50,  50,  5},
        {"RespiratoryRate",   30,  30,  3},
        {"RespiratoryRate_2", 30,  30,  3},
        {"BloodPressure",    100, 100, 10},
        {"BloodPressure_2",  100, 100, 10},
        {"EEG_Alpha",        100, 100, 10},
        {"CO2_Level",         40,  40,  4},
        {"CO2_Level_2",       40,  40,  4},
        {"EMG_Muscle",        50,  50,  5},
        {"Temperature",      200, 200, 15},
        {"Glucose",           500, 500, 25},
    };
    int n = (int)(sizeof(cfg) / sizeof(cfg[0]));

    for (int i = 0; i < n; i++) {
        strncpy(sensors[i].name, cfg[i].name, MAX_NAME_LEN - 1);
        sensors[i].name[MAX_NAME_LEN - 1] = '\0';
        sensors[i].id = i;
        sensors[i].period_ms   = cfg[i].period;
        sensors[i].deadline_ms = cfg[i].deadline;
        sensors[i].exec_ms     = cfg[i].exec;
        /* No explicit priority table for Phase 2 in the original test plan,
         * so we derive it the same way real-time systems usually do:
         * shorter deadline => higher urgency => higher priority. */
        int prio = 99 - (cfg[i].deadline / 2);
        if (prio < 10) prio = 10;
        if (prio > 99) prio = 99;
        sensors[i].priority = prio;
    }
    return n;
}

/* ---- Realistic value generation ----------------------------------------- */
void sensor_generate_reading(const sensor_t *s, double *out_value, int *out_alarm)
{
    double v = 0.0;
    int alarm = 0;

    /* crude but plausible physiological ranges, keyed by name prefix */
    if (strncmp(s->name, "ECG", 3) == 0) {
        v = 60.0 + (rand() % 401) / 10.0;      /* 60-100 bpm typical */
        if (rand() % 100 < 3) { v += 40.0; alarm = 1; }  /* rare tachycardia spike */
    } else if (strncmp(s->name, "SpO2", 4) == 0) {
        v = 94.0 + (rand() % 61) / 10.0;       /* 94-100 % */
        if (rand() % 100 < 3) { v -= 10.0; alarm = 1; }  /* desaturation event */
    } else if (strncmp(s->name, "Respiratory", 11) == 0) {
        v = 12.0 + (rand() % 91) / 10.0;       /* 12-21 breaths/min */
    } else if (strncmp(s->name, "BloodPressure", 13) == 0 || strncmp(s->name, "Pression", 8) == 0) {
        v = 110.0 + (rand() % 300) / 10.0;     /* systolic 110-140 */
        if (rand() % 100 < 3) { v += 40.0; alarm = 1; }  /* hypertensive spike */
    } else if (strncmp(s->name, "EEG", 3) == 0) {
        v = (rand() % 1000) / 100.0;           /* alpha wave amplitude (uV) */
    } else if (strncmp(s->name, "CO2", 3) == 0) {
        v = 35.0 + (rand() % 100) / 10.0;      /* 35-45 mmHg */
    } else if (strncmp(s->name, "EMG", 3) == 0) {
        v = (rand() % 500) / 10.0;             /* muscle activity, arbitrary units */
    } else if (strncmp(s->name, "Temperature", 11) == 0) {
        v = 36.0 + (rand() % 250) / 100.0;     /* 36.0-38.5 C */
        if (v > 38.0) alarm = 1;
    } else if (strncmp(s->name, "Glucose", 7) == 0) {
        v = 70.0 + (rand() % 1300) / 10.0;     /* 70-200 mg/dL */
        if (v > 180.0) alarm = 1;
    } else {
        v = (double)(rand() % 100);
    }

    *out_value = v;
    *out_alarm = alarm;
}
