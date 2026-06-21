#define _POSIX_C_SOURCE 200809L
#include "deadlock_core.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define LINE_BUFFER_SIZE 4096

static void set_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s", message);
    }
}

static char *trim(char *text) {
    char *end;
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)*(end - 1))) {
        --end;
    }
    *end = '\0';
    return text;
}

static int compare_events_by_time(const void *left, const void *right) {
    const Event *a = (const Event *)left;
    const Event *b = (const Event *)right;
    if (a->time < b->time) return -1;
    if (a->time > b->time) return 1;
    return 0;
}

static int grow_event_array(Dataset *dataset, size_t *capacity) {
    Event *new_events;
    size_t new_capacity = (*capacity == 0) ? 64 : (*capacity * 2);
    new_events = realloc(dataset->events, new_capacity * sizeof(Event));
    if (new_events == NULL) {
        return 0;
    }
    dataset->events = new_events;
    *capacity = new_capacity;
    return 1;
}

static int add_or_find_id(char ***ids, int *count, int *capacity, const char *id) {
    int i;
    char **new_ids;
    char *copy;

    for (i = 0; i < *count; ++i) {
        if (strcmp((*ids)[i], id) == 0) {
            return i;
        }
    }

    if (*count == *capacity) {
        int new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        new_ids = realloc(*ids, (size_t)new_capacity * sizeof(char *));
        if (new_ids == NULL) {
            return -1;
        }
        *ids = new_ids;
        *capacity = new_capacity;
    }

    copy = strdup(id);
    if (copy == NULL) {
        return -1;
    }
    (*ids)[*count] = copy;
    ++(*count);
    return *count - 1;
}

static int parse_header(char *line) {
    char *save = NULL;
    char *fields[4] = {NULL, NULL, NULL, NULL};
    int index = 0;
    char *token = strtok_r(line, ",", &save);

    while (token != NULL && index < 4) {
        fields[index++] = trim(token);
        token = strtok_r(NULL, ",", &save);
    }

    return index == 4 &&
           strcmp(fields[0], "time") == 0 &&
           strcmp(fields[1], "process_id") == 0 &&
           strcmp(fields[2], "action") == 0 &&
           strcmp(fields[3], "resource_id") == 0;
}

int read_dataset_csv(const char *path, Dataset *dataset, char *error, size_t error_size) {
    FILE *file;
    char line[LINE_BUFFER_SIZE];
    size_t event_capacity = 0;
    int process_capacity = 0;
    int resource_capacity = 0;
    int line_number = 0;

    if (dataset == NULL) {
        set_error(error, error_size, "Dataset output pointer is NULL.");
        return 0;
    }
    memset(dataset, 0, sizeof(*dataset));

    file = fopen(path, "r");
    if (file == NULL) {
        snprintf(error, error_size, "Cannot open CSV file: %s", path);
        return 0;
    }

    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        set_error(error, error_size, "CSV file is empty.");
        return 0;
    }
    ++line_number;
    if (!parse_header(line)) {
        fclose(file);
        set_error(error, error_size,
                  "Invalid CSV header. Expected: time,process_id,action,resource_id");
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *save = NULL;
        char *fields[4] = {NULL, NULL, NULL, NULL};
        char *token;
        int index = 0;
        char *endptr;
        long logical_time;
        int process_index;
        int resource_index;

        ++line_number;
        if (strchr(line, '\n') == NULL && !feof(file)) {
            fclose(file);
            set_error(error, error_size, "CSV line exceeds supported length.");
            free_dataset(dataset);
            return 0;
        }

        token = strtok_r(line, ",", &save);
        while (token != NULL && index < 4) {
            fields[index++] = trim(token);
            token = strtok_r(NULL, ",", &save);
        }

        if (index != 4 || token != NULL) {
            snprintf(error, error_size, "Invalid CSV row at line %d.", line_number);
            fclose(file);
            free_dataset(dataset);
            return 0;
        }

        errno = 0;
        logical_time = strtol(fields[0], &endptr, 10);
        if (errno != 0 || *trim(endptr) != '\0') {
            snprintf(error, error_size, "Invalid time value at line %d.", line_number);
            fclose(file);
            free_dataset(dataset);
            return 0;
        }
        if (fields[1][0] == '\0' || fields[3][0] == '\0') {
            snprintf(error, error_size, "Missing process_id or resource_id at line %d.", line_number);
            fclose(file);
            free_dataset(dataset);
            return 0;
        }
        if (strcasecmp(fields[2], "request") != 0) {
            snprintf(error, error_size,
                     "Unsupported action at line %d. Topic 3 accepts only request.", line_number);
            fclose(file);
            free_dataset(dataset);
            return 0;
        }

        process_index = add_or_find_id(&dataset->process_ids, &dataset->process_count,
                                       &process_capacity, fields[1]);
        resource_index = add_or_find_id(&dataset->resource_ids, &dataset->resource_count,
                                        &resource_capacity, fields[3]);
        if (process_index < 0 || resource_index < 0) {
            fclose(file);
            free_dataset(dataset);
            set_error(error, error_size, "Out of memory while reading CSV IDs.");
            return 0;
        }

        if (dataset->event_count == event_capacity && !grow_event_array(dataset, &event_capacity)) {
            fclose(file);
            free_dataset(dataset);
            set_error(error, error_size, "Out of memory while reading CSV events.");
            return 0;
        }

        dataset->events[dataset->event_count].time = (int)logical_time;
        dataset->events[dataset->event_count].process = process_index;
        dataset->events[dataset->event_count].resource = resource_index;
        ++dataset->event_count;
    }

    fclose(file);
    qsort(dataset->events, dataset->event_count, sizeof(Event), compare_events_by_time);
    return 1;
}

void free_dataset(Dataset *dataset) {
    int i;
    if (dataset == NULL) {
        return;
    }
    free(dataset->events);
    for (i = 0; i < dataset->process_count; ++i) {
        free(dataset->process_ids[i]);
    }
    for (i = 0; i < dataset->resource_count; ++i) {
        free(dataset->resource_ids[i]);
    }
    free(dataset->process_ids);
    free(dataset->resource_ids);
    memset(dataset, 0, sizeof(*dataset));
}

int parse_algorithm(const char *name, Algorithm *algorithm) {
    if (name == NULL || algorithm == NULL) {
        return 0;
    }
    if (strcasecmp(name, "dfs") == 0) {
        *algorithm = ALGO_DFS;
        return 1;
    }
    if (strcasecmp(name, "bfs") == 0) {
        *algorithm = ALGO_BFS;
        return 1;
    }
    return 0;
}

const char *algorithm_name(Algorithm algorithm) {
    return algorithm == ALGO_BFS ? "bfs" : "dfs";
}

typedef struct {
    const unsigned char *adjacency;
    int n;
    int *color;
    int *stack;
    int *stack_index;
    int stack_size;
    CycleResult *result;
} DfsContext;

static int dfs_visit(DfsContext *context, int node) {
    int neighbor;
    context->color[node] = 1;
    context->stack_index[node] = context->stack_size;
    context->stack[context->stack_size++] = node;

    for (neighbor = 0; neighbor < context->n; ++neighbor) {
        int state;
        if (context->adjacency[(size_t)node * (size_t)context->n + (size_t)neighbor] == 0) {
            continue;
        }
        state = context->color[neighbor];
        if (state == 0) {
            if (dfs_visit(context, neighbor)) {
                return 1;
            }
        } else if (state == 1) {
            int start = context->stack_index[neighbor];
            int i;
            context->result->has_deadlock = 1;
            context->result->cycle_length = 0;
            for (i = start; i < context->stack_size && context->result->cycle_length < 511; ++i) {
                context->result->cycle_nodes[context->result->cycle_length++] = context->stack[i];
            }
            context->result->cycle_nodes[context->result->cycle_length++] = neighbor;
            return 1;
        }
    }

    --context->stack_size;
    context->stack_index[node] = -1;
    context->color[node] = 2;
    return 0;
}

int detect_cycle_dfs(const unsigned char *adjacency, int n, CycleResult *result) {
    int *color;
    int *stack;
    int *stack_index;
    int node;
    DfsContext context;

    if (result == NULL || adjacency == NULL || n < 0) {
        return 0;
    }
    memset(result, 0, sizeof(*result));
    if (n == 0) {
        return 0;
    }

    color = calloc((size_t)n, sizeof(int));
    stack = malloc((size_t)n * sizeof(int));
    stack_index = malloc((size_t)n * sizeof(int));
    if (color == NULL || stack == NULL || stack_index == NULL) {
        free(color);
        free(stack);
        free(stack_index);
        return 0;
    }
    for (node = 0; node < n; ++node) {
        stack_index[node] = -1;
    }

    context.adjacency = adjacency;
    context.n = n;
    context.color = color;
    context.stack = stack;
    context.stack_index = stack_index;
    context.stack_size = 0;
    context.result = result;

    for (node = 0; node < n; ++node) {
        if (color[node] == 0 && dfs_visit(&context, node)) {
            free(color);
            free(stack);
            free(stack_index);
            return 1;
        }
    }

    free(color);
    free(stack);
    free(stack_index);
    return 0;
}

int detect_cycle_bfs(const unsigned char *adjacency, int n, CycleResult *result) {
    int *indegree;
    int *queue;
    int front = 0;
    int back = 0;
    int removed = 0;
    int source;

    if (result == NULL || adjacency == NULL || n < 0) {
        return 0;
    }
    memset(result, 0, sizeof(*result));
    if (n == 0) {
        return 0;
    }

    indegree = calloc((size_t)n, sizeof(int));
    queue = malloc((size_t)n * sizeof(int));
    if (indegree == NULL || queue == NULL) {
        free(indegree);
        free(queue);
        return 0;
    }

    for (source = 0; source < n; ++source) {
        int target;
        for (target = 0; target < n; ++target) {
            if (adjacency[(size_t)source * (size_t)n + (size_t)target]) {
                ++indegree[target];
            }
        }
    }
    for (source = 0; source < n; ++source) {
        if (indegree[source] == 0) {
            queue[back++] = source;
        }
    }

    while (front < back) {
        int node = queue[front++];
        int target;
        ++removed;
        for (target = 0; target < n; ++target) {
            if (adjacency[(size_t)node * (size_t)n + (size_t)target]) {
                --indegree[target];
                if (indegree[target] == 0) {
                    queue[back++] = target;
                }
            }
        }
    }

    free(indegree);
    free(queue);

    if (removed == n) {
        return 0;
    }

    /* Kahn's algorithm established that a cycle exists. Extract a concrete cycle for trace output. */
    return detect_cycle_dfs(adjacency, n, result);
}

static double now_seconds(void) {
    struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return (double)timestamp.tv_sec + (double)timestamp.tv_nsec / 1000000000.0;
}

static void build_wait_for_graph(const int *resource_owner,
                                 const unsigned char *waiting,
                                 int process_count,
                                 int resource_count,
                                 unsigned char *adjacency) {
    int process;
    memset(adjacency, 0, (size_t)process_count * (size_t)process_count);
    for (process = 0; process < process_count; ++process) {
        int resource;
        for (resource = 0; resource < resource_count; ++resource) {
            int owner;
            if (waiting[(size_t)process * (size_t)resource_count + (size_t)resource] == 0) {
                continue;
            }
            owner = resource_owner[resource];
            if (owner >= 0 && owner != process) {
                adjacency[(size_t)process * (size_t)process_count + (size_t)owner] = 1;
            }
        }
    }
}

static void graph_size(const unsigned char *adjacency, int n, int *node_count, int *edge_count) {
    unsigned char *active;
    int source;
    int edges = 0;
    int nodes = 0;

    active = calloc((size_t)n, sizeof(unsigned char));
    if (active == NULL) {
        *node_count = 0;
        *edge_count = 0;
        return;
    }

    for (source = 0; source < n; ++source) {
        int target;
        for (target = 0; target < n; ++target) {
            if (adjacency[(size_t)source * (size_t)n + (size_t)target]) {
                ++edges;
                active[source] = 1;
                active[target] = 1;
            }
        }
    }
    for (source = 0; source < n; ++source) {
        if (active[source]) {
            ++nodes;
        }
    }
    free(active);
    *node_count = nodes;
    *edge_count = edges;
}

static void print_trace_graph(const Dataset *dataset, const unsigned char *adjacency) {
    int source;
    int printed = 0;
    printf("  WFG  : ");
    for (source = 0; source < dataset->process_count; ++source) {
        int target;
        for (target = 0; target < dataset->process_count; ++target) {
            if (adjacency[(size_t)source * (size_t)dataset->process_count + (size_t)target]) {
                if (printed) {
                    printf(", ");
                }
                printf("%s->%s", dataset->process_ids[source], dataset->process_ids[target]);
                printed = 1;
            }
        }
    }
    if (!printed) {
        printf("{}");
    }
    printf("\n");
}

static void print_trace_owners(const Dataset *dataset, const int *resource_owner) {
    int resource;
    int printed = 0;
    printf("  owner: {");
    for (resource = 0; resource < dataset->resource_count; ++resource) {
        if (resource_owner[resource] >= 0) {
            if (printed) {
                printf(", ");
            }
            printf("%s:%s", dataset->resource_ids[resource],
                   dataset->process_ids[resource_owner[resource]]);
            printed = 1;
        }
    }
    printf("}\n");
}

int simulate_dataset(const Dataset *dataset,
                     int detection_interval,
                     Algorithm algorithm,
                     int trace,
                     SimulationResult *result,
                     char *error,
                     size_t error_size) {
    int *resource_owner = NULL;
    unsigned char *waiting = NULL;
    unsigned char *adjacency = NULL;
    size_t event_index;
    int i;
    double total_detection_time = 0.0;
    int detection_frequency = 0;
    int first_actual = -1;
    int first_detected = -1;
    int max_nodes = 0;
    int max_edges = 0;
    CycleResult last_cycle;

    if (dataset == NULL || result == NULL || detection_interval < 1) {
        set_error(error, error_size, "Invalid simulation arguments.");
        return 0;
    }
    memset(result, 0, sizeof(*result));
    memset(&last_cycle, 0, sizeof(last_cycle));
    result->first_actual_deadlock_time = -1;
    result->first_detected_deadlock_time = -1;
    result->detection_latency = -1;
    result->algorithm = algorithm;
    result->detection_interval = detection_interval;
    result->events = (int)dataset->event_count;

    resource_owner = malloc((size_t)dataset->resource_count * sizeof(int));
    waiting = calloc((size_t)dataset->process_count * (size_t)dataset->resource_count,
                     sizeof(unsigned char));
    adjacency = calloc((size_t)dataset->process_count * (size_t)dataset->process_count,
                       sizeof(unsigned char));
    if ((dataset->resource_count > 0 && resource_owner == NULL) ||
        (dataset->process_count > 0 && dataset->resource_count > 0 && waiting == NULL) ||
        (dataset->process_count > 0 && adjacency == NULL)) {
        free(resource_owner);
        free(waiting);
        free(adjacency);
        set_error(error, error_size, "Out of memory during simulation.");
        return 0;
    }
    for (i = 0; i < dataset->resource_count; ++i) {
        resource_owner[i] = -1;
    }

    for (event_index = 0; event_index < dataset->event_count; ++event_index) {
        const Event *event = &dataset->events[event_index];
        int owner = resource_owner[event->resource];
        int node_count;
        int edge_count;
        CycleResult actual;

        if (owner < 0) {
            resource_owner[event->resource] = event->process;
        } else if (owner != event->process) {
            waiting[(size_t)event->process * (size_t)dataset->resource_count +
                    (size_t)event->resource] = 1;
        }

        build_wait_for_graph(resource_owner, waiting, dataset->process_count,
                             dataset->resource_count, adjacency);
        graph_size(adjacency, dataset->process_count, &node_count, &edge_count);
        if (node_count > max_nodes) max_nodes = node_count;
        if (edge_count > max_edges) max_edges = edge_count;

        /* Offline oracle: locate the logical time at which a cycle first becomes true. */
        if (first_actual < 0 && detect_cycle_dfs(adjacency, dataset->process_count, &actual)) {
            first_actual = event->time;
        }

        if (((int)event_index + 1) % detection_interval == 0 ||
            event_index + 1 == dataset->event_count) {
            CycleResult scheduled;
            int has_cycle;
            double begin = now_seconds();
            if (algorithm == ALGO_DFS) {
                has_cycle = detect_cycle_dfs(adjacency, dataset->process_count, &scheduled);
            } else {
                has_cycle = detect_cycle_bfs(adjacency, dataset->process_count, &scheduled);
            }
            total_detection_time += now_seconds() - begin;
            ++detection_frequency;
            if (has_cycle) {
                last_cycle = scheduled;
                if (first_detected < 0) {
                    first_detected = event->time;
                }
            }
        }

        if (trace) {
            printf("time=%4d event=%s request %s\n", event->time,
                   dataset->process_ids[event->process], dataset->resource_ids[event->resource]);
            print_trace_owners(dataset, resource_owner);
            print_trace_graph(dataset, adjacency);
        }
    }

    result->detection_frequency = detection_frequency;
    result->total_detection_time_seconds = total_detection_time;
    result->avg_detection_time_seconds = detection_frequency > 0
        ? total_detection_time / (double)detection_frequency : 0.0;
    result->detection_overhead_seconds = total_detection_time;
    result->first_actual_deadlock_time = first_actual;
    result->first_detected_deadlock_time = first_detected;
    result->detection_latency = (first_actual >= 0 && first_detected >= 0)
        ? first_detected - first_actual : -1;
    result->deadlock_detected = first_detected >= 0;
    result->max_wait_for_nodes = max_nodes;
    result->max_wait_for_edges = max_edges;
    result->last_cycle = last_cycle;

    free(resource_owner);
    free(waiting);
    free(adjacency);
    return 1;
}

void print_simulation_result(const Dataset *dataset, const SimulationResult *result) {
    int i;
    printf("events: %d\n", result->events);
    printf("detector: %s\n", algorithm_name(result->algorithm));
    printf("detection_interval: %d\n", result->detection_interval);
    printf("detection_frequency: %d\n", result->detection_frequency);
    printf("avg_detection_time_seconds: %.9f\n", result->avg_detection_time_seconds);
    printf("total_detection_time_seconds: %.9f\n", result->total_detection_time_seconds);
    printf("detection_overhead_seconds: %.9f\n", result->detection_overhead_seconds);
    printf("first_actual_deadlock_time: %d\n", result->first_actual_deadlock_time);
    printf("first_detected_deadlock_time: %d\n", result->first_detected_deadlock_time);
    printf("detection_latency: %d\n", result->detection_latency);
    printf("deadlock_detected: %s\n", result->deadlock_detected ? "True" : "False");
    printf("max_wait_for_nodes: %d\n", result->max_wait_for_nodes);
    printf("max_wait_for_edges: %d\n", result->max_wait_for_edges);
    printf("cycle: ");
    if (result->last_cycle.cycle_length == 0) {
        printf("\n");
        return;
    }
    for (i = 0; i < result->last_cycle.cycle_length; ++i) {
        if (i > 0) printf(" -> ");
        if (dataset != NULL && result->last_cycle.cycle_nodes[i] >= 0 &&
            result->last_cycle.cycle_nodes[i] < dataset->process_count) {
            printf("%s", dataset->process_ids[result->last_cycle.cycle_nodes[i]]);
        } else {
            printf("P%d", result->last_cycle.cycle_nodes[i]);
        }
    }
    printf("\n");
}
