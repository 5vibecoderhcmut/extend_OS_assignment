#ifndef DEADLOCK_CORE_H
#define DEADLOCK_CORE_H

#include <stddef.h>

#define ID_MAX_LEN 64

typedef enum {
    ALGO_DFS = 0,
    ALGO_BFS = 1
} Algorithm;

typedef struct {
    int time;
    int process;
    int resource;
} Event;

typedef struct {
    Event *events;
    size_t event_count;
    char **process_ids;
    int process_count;
    char **resource_ids;
    int resource_count;
} Dataset;

typedef struct {
    int has_deadlock;
    int cycle_nodes[512];
    int cycle_length;
} CycleResult;

typedef struct {
    int events;
    Algorithm algorithm;
    int detection_interval;
    int detection_frequency;
    double avg_detection_time_seconds;
    double total_detection_time_seconds;
    double detection_overhead_seconds;
    int first_actual_deadlock_time;   /* -1 when no deadlock occurs */
    int first_detected_deadlock_time; /* -1 when no scheduled detection finds a deadlock */
    int detection_latency;            /* -1 when unavailable */
    int deadlock_detected;
    int max_wait_for_nodes;
    int max_wait_for_edges;
    CycleResult last_cycle;
} SimulationResult;

/* Dataset and utility functions */
int read_dataset_csv(const char *path, Dataset *dataset, char *error, size_t error_size);
void free_dataset(Dataset *dataset);

int parse_algorithm(const char *name, Algorithm *algorithm);
const char *algorithm_name(Algorithm algorithm);

/* Cycle detection over an n x n adjacency matrix in row-major order. */
int detect_cycle_dfs(const unsigned char *adjacency, int n, CycleResult *result);
int detect_cycle_bfs(const unsigned char *adjacency, int n, CycleResult *result);

/* Event-driven resource simulator. */
int simulate_dataset(const Dataset *dataset,
                     int detection_interval,
                     Algorithm algorithm,
                     int trace,
                     SimulationResult *result,
                     char *error,
                     size_t error_size);

void print_simulation_result(const Dataset *dataset, const SimulationResult *result);

#endif
