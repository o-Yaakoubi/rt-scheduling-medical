#include <stdio.h>
#include <string.h>
#include "metrics.h"

void metrics_init(sensor_metrics_t *m, const sensor_t *sensors, int n)
{
    for (int i = 0; i < n; i++) {
        memset(&m[i], 0, sizeof(m[i]));
        m[i].sensor_id   = sensors[i].id;
        strncpy(m[i].name, sensors[i].name, MAX_NAME_LEN - 1);
        m[i].period_ms   = sensors[i].period_ms;
        m[i].deadline_ms = sensors[i].deadline_ms;
    }
}

void metrics_record(sensor_metrics_t *m, int n, int sensor_id,
                     double latency_ms, int missed)
{
    for (int i = 0; i < n; i++) {
        if (m[i].sensor_id == sensor_id) {
            m[i].exec_count++;
            m[i].latency_sum_ms += latency_ms;
            if (missed) m[i].miss_count++;
            return;
        }
    }
}

void metrics_print_report(const sensor_metrics_t *m, int n, const char *policy_name)
{
    long total_exec = 0, total_miss = 0;

    printf("\n=================================================================\n");
    printf(" Rapport d'execution - Ordonnanceur : %s\n", policy_name);
    printf("=================================================================\n");
    printf("%-18s %10s %8s %8s %12s\n",
           "Capteur", "Exec", "Misses", "Taux%", "Latence(ms)");
    printf("-----------------------------------------------------------------\n");

    for (int i = 0; i < n; i++) {
        double taux = (m[i].exec_count > 0)
                        ? (100.0 * m[i].miss_count / m[i].exec_count) : 0.0;
        double lat_avg = (m[i].exec_count > 0)
                        ? (m[i].latency_sum_ms / m[i].exec_count) : 0.0;
        printf("%-18s %10ld %8ld %7.2f%% %12.2f\n",
               m[i].name, m[i].exec_count, m[i].miss_count, taux, lat_avg);
        total_exec += m[i].exec_count;
        total_miss += m[i].miss_count;
    }

    double taux_reussite = (total_exec > 0)
                        ? (100.0 * (total_exec - total_miss) / total_exec) : 0.0;

    printf("-----------------------------------------------------------------\n");
    printf(" %s Global : %ld executions | %ld Misses | Taux reussite = %.2f%%\n",
           policy_name, total_exec, total_miss, taux_reussite);
    printf("=================================================================\n\n");
}

int metrics_write_csv(const char *filename, const sensor_metrics_t *m, int n,
                       const char *policy_name)
{
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        return -1;
    }

    fprintf(f, "scheduler,sensor,period_ms,deadline_ms,exec_count,miss_count,taux_pct,latence_moy_ms\n");
    for (int i = 0; i < n; i++) {
        double taux = (m[i].exec_count > 0)
                        ? (100.0 * m[i].miss_count / m[i].exec_count) : 0.0;
        double lat_avg = (m[i].exec_count > 0)
                        ? (m[i].latency_sum_ms / m[i].exec_count) : 0.0;
        fprintf(f, "%s,%s,%d,%d,%ld,%ld,%.2f,%.2f\n",
                policy_name, m[i].name, m[i].period_ms, m[i].deadline_ms,
                m[i].exec_count, m[i].miss_count, taux, lat_avg);
    }

    fclose(f);
    return 0;
}
