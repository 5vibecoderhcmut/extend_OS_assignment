#define _POSIX_C_SOURCE 200809L
#include "deadlock_core.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_ALGORITHMS 2
#define MAX_K_VALUES 32
#define MAX_GROUP_FILTERS 16

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} StringList;

typedef struct {
    char dataset_group[32];
    char dataset_file[PATH_MAX];
    double process_count;
    double resource_count;
    double event_count;
    char algorithm[8];
    int detection_interval;
    int runs;
    int deadlock_detected;
    double actual_deadlock_time;
    int actual_deadlock_available;
    double detected_deadlock_time;
    int detected_deadlock_available;
    double detection_latency;
    int detection_latency_available;
    double detection_frequency;
    double detection_time_seconds;
    double detection_overhead_seconds;
    double max_wait_for_edges;
} ResultRow;

typedef struct {
    ResultRow *rows;
    size_t count;
    size_t capacity;
} ResultList;

typedef struct {
    const char *dataset_dir;
    const char *output_dir;
    Algorithm algorithms[MAX_ALGORITHMS];
    int algorithm_count;
    int k_values[MAX_K_VALUES];
    int k_count;
    int repeat;
    const char *groups[MAX_GROUP_FILTERS];
    int group_count;
    int fail_fast;
} Arguments;

static const char *GROUP_NAMES[] = {"small", "medium", "large", "very_large"};

static void usage(const char *program) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  --dataset-dir <dir>       Dataset root directory (default: datasets)\n"
            "  --output-dir <dir>        Result directory (default: results)\n"
            "  --algorithms dfs bfs      Algorithms to run (default: dfs bfs)\n"
            "  --k 1 2 5 10              Detection intervals (default: 1 2 5 10)\n"
            "  --repeat <n>              Timing repetitions per configuration (default: 5)\n"
            "  --groups small medium     Optional group filter\n"
            "  --fail-fast               Stop after the first invalid dataset\n",
            program);
}

static int ensure_directory(const char *path) {
    char buffer[PATH_MAX];
    size_t length;
    char *cursor;

    if (path == NULL || path[0] == '\0') return 0;
    if (strlen(path) >= sizeof(buffer)) return 0;
    strcpy(buffer, path);
    length = strlen(buffer);
    if (length == 0) return 0;
    if (buffer[length - 1] == '/') buffer[length - 1] = '\0';

    for (cursor = buffer + 1; *cursor != '\0'; ++cursor) {
        if (*cursor == '/') {
            *cursor = '\0';
            if (mkdir(buffer, 0775) != 0 && errno != EEXIST) return 0;
            *cursor = '/';
        }
    }
    if (mkdir(buffer, 0775) != 0 && errno != EEXIST) return 0;
    return 1;
}

static int is_directory(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static int ends_with(const char *text, const char *suffix) {
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length && strcmp(text + text_length - suffix_length, suffix) == 0;
}

static int excluded_csv_name(const char *name) {
    static const char *excluded[] = {
        "experiment_results.csv", "group_summary.csv", "experiment_runs.csv",
        "experiment_summary.csv", "expected_results.csv", "manifest_expected_results.csv"
    };
    size_t i;
    for (i = 0; i < sizeof(excluded) / sizeof(excluded[0]); ++i) {
        if (strcmp(name, excluded[i]) == 0) return 1;
    }
    return 0;
}

static int add_string(StringList *list, const char *value) {
    char **new_items;
    char *copy;
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 32 : list->capacity * 2;
        new_items = realloc(list->items, new_capacity * sizeof(char *));
        if (new_items == NULL) return 0;
        list->items = new_items;
        list->capacity = new_capacity;
    }
    copy = strdup(value);
    if (copy == NULL) return 0;
    list->items[list->count++] = copy;
    return 1;
}

static void free_string_list(StringList *list) {
    size_t i;
    for (i = 0; i < list->count; ++i) free(list->items[i]);
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static int compare_string_ptr(const void *left, const void *right) {
    const char *const *a = (const char *const *)left;
    const char *const *b = (const char *const *)right;
    return strcmp(*a, *b);
}

static void normalize_path_separators(char *path) {
    char *cursor;
    for (cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '\\') *cursor = '/';
    }
}

static void relative_path(const char *base, const char *full, char *out, size_t out_size) {
    size_t base_length = strlen(base);
    const char *start = full;
    if (strncmp(base, full, base_length) == 0) {
        start = full + base_length;
        if (*start == '/') ++start;
    }
    snprintf(out, out_size, "%s", start);
    normalize_path_separators(out);
}

static void group_from_relative_path(const char *relative, char *group, size_t group_size) {
    const char *slash = strchr(relative, '/');
    size_t length = slash == NULL ? strlen(relative) : (size_t)(slash - relative);
    if (length >= group_size) length = group_size - 1;
    memcpy(group, relative, length);
    group[length] = '\0';
}

static int group_allowed(const Arguments *args, const char *group) {
    int i;
    if (args->group_count == 0) return 1;
    for (i = 0; i < args->group_count; ++i) {
        if (strcmp(args->groups[i], group) == 0) return 1;
    }
    return 0;
}

static int scan_dataset_files(const char *base,
                              const char *current,
                              const Arguments *args,
                              StringList *files) {
    DIR *directory;
    struct dirent *entry;

    directory = opendir(current);
    if (directory == NULL) return 0;

    while ((entry = readdir(directory)) != NULL) {
        char full_path[PATH_MAX];
        struct stat info;
        char relative[PATH_MAX];
        char group[32];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (snprintf(full_path, sizeof(full_path), "%s/%s", current, entry->d_name) >= (int)sizeof(full_path)) {
            closedir(directory);
            return 0;
        }
        if (stat(full_path, &info) != 0) continue;

        if (S_ISDIR(info.st_mode)) {
            if (!scan_dataset_files(base, full_path, args, files)) {
                closedir(directory);
                return 0;
            }
        } else if (S_ISREG(info.st_mode) && ends_with(entry->d_name, ".csv") &&
                   !excluded_csv_name(entry->d_name)) {
            relative_path(base, full_path, relative, sizeof(relative));
            group_from_relative_path(relative, group, sizeof(group));
            if (group_allowed(args, group) && !add_string(files, full_path)) {
                closedir(directory);
                return 0;
            }
        }
    }

    closedir(directory);
    return 1;
}

static int add_result(ResultList *list, const ResultRow *row) {
    ResultRow *new_rows;
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 128 : list->capacity * 2;
        new_rows = realloc(list->rows, new_capacity * sizeof(ResultRow));
        if (new_rows == NULL) return 0;
        list->rows = new_rows;
        list->capacity = new_capacity;
    }
    list->rows[list->count++] = *row;
    return 1;
}

static void free_result_list(ResultList *list) {
    free(list->rows);
    memset(list, 0, sizeof(*list));
}

static int group_order(const char *group) {
    size_t i;
    for (i = 0; i < sizeof(GROUP_NAMES) / sizeof(GROUP_NAMES[0]); ++i) {
        if (strcmp(group, GROUP_NAMES[i]) == 0) return (int)i;
    }
    return 99;
}

static int compare_result_rows(const void *left, const void *right) {
    const ResultRow *a = (const ResultRow *)left;
    const ResultRow *b = (const ResultRow *)right;
    int order_a = group_order(a->dataset_group);
    int order_b = group_order(b->dataset_group);
    int compare;
    if (order_a != order_b) return order_a - order_b;
    compare = strcmp(a->dataset_file, b->dataset_file);
    if (compare != 0) return compare;
    compare = strcmp(a->algorithm, b->algorithm);
    if (compare != 0) return compare;
    return a->detection_interval - b->detection_interval;
}

static int run_configuration(const char *dataset_file,
                             const char *dataset_dir,
                             const Dataset *dataset,
                             Algorithm algorithm,
                             int k,
                             int repeat,
                             ResultRow *row,
                             char *error,
                             size_t error_size) {
    int iteration;
    SimulationResult first_result;
    double time_sum = 0.0;
    double overhead_sum = 0.0;
    char relative[PATH_MAX];

    memset(row, 0, sizeof(*row));
    memset(&first_result, 0, sizeof(first_result));

    for (iteration = 0; iteration < repeat; ++iteration) {
        SimulationResult result;
        if (!simulate_dataset(dataset, k, algorithm, 0, &result, error, error_size)) {
            return 0;
        }
        if (iteration == 0) first_result = result;
        time_sum += result.avg_detection_time_seconds;
        overhead_sum += result.detection_overhead_seconds;
    }

    relative_path(dataset_dir, dataset_file, relative, sizeof(relative));
    group_from_relative_path(relative, row->dataset_group, sizeof(row->dataset_group));
    snprintf(row->dataset_file, sizeof(row->dataset_file), "%s", relative);
    row->process_count = (double)dataset->process_count;
    row->resource_count = (double)dataset->resource_count;
    row->event_count = (double)dataset->event_count;
    snprintf(row->algorithm, sizeof(row->algorithm), "%s", algorithm_name(algorithm));
    row->detection_interval = k;
    row->runs = repeat;
    row->deadlock_detected = first_result.deadlock_detected;
    row->actual_deadlock_available = first_result.first_actual_deadlock_time >= 0;
    row->actual_deadlock_time = first_result.first_actual_deadlock_time;
    row->detected_deadlock_available = first_result.first_detected_deadlock_time >= 0;
    row->detected_deadlock_time = first_result.first_detected_deadlock_time;
    row->detection_latency_available = first_result.detection_latency >= 0;
    row->detection_latency = first_result.detection_latency;
    row->detection_frequency = (double)first_result.detection_frequency;
    row->detection_time_seconds = time_sum / (double)repeat;
    row->detection_overhead_seconds = overhead_sum / (double)repeat;
    row->max_wait_for_edges = (double)first_result.max_wait_for_edges;
    return 1;
}

static void write_optional_number(FILE *file, int available, double value) {
    if (available) fprintf(file, "%.3f", value);
}

static int write_result_csv(const char *path, const ResultList *results) {
    FILE *file;
    size_t i;
    file = fopen(path, "w");
    if (file == NULL) return 0;
    fprintf(file,
            "dataset_group,dataset_file,process_count,resource_count,event_count,algorithm,"
            "detection_interval,runs,deadlock_detected,actual_deadlock_time,"
            "detected_deadlock_time,detection_latency,detection_frequency,"
            "detection_time_seconds,detection_overhead_seconds,max_wait_for_edges\n");
    for (i = 0; i < results->count; ++i) {
        const ResultRow *row = &results->rows[i];
        fprintf(file, "%s,%s,%.3f,%.3f,%.3f,%s,%d,%d,%s,",
                row->dataset_group, row->dataset_file, row->process_count,
                row->resource_count, row->event_count, row->algorithm,
                row->detection_interval, row->runs,
                row->deadlock_detected ? "True" : "False");
        write_optional_number(file, row->actual_deadlock_available, row->actual_deadlock_time);
        fputc(',', file);
        write_optional_number(file, row->detected_deadlock_available, row->detected_deadlock_time);
        fputc(',', file);
        write_optional_number(file, row->detection_latency_available, row->detection_latency);
        fprintf(file, ",%.3f,%.9f,%.9f,%.3f\n",
                row->detection_frequency, row->detection_time_seconds,
                row->detection_overhead_seconds, row->max_wait_for_edges);
    }
    fclose(file);
    return 1;
}

static int same_summary_key(const ResultRow *row, const char *group, const char *algorithm, int k) {
    return strcmp(row->dataset_group, group) == 0 &&
           strcmp(row->algorithm, algorithm) == 0 &&
           row->detection_interval == k;
}

static ResultRow average_group_rows(const ResultList *results,
                                    const char *group,
                                    const char *algorithm,
                                    int k) {
    ResultRow summary;
    size_t i;
    int count = 0;
    int actual_count = 0;
    int detected_count = 0;
    int latency_count = 0;
    memset(&summary, 0, sizeof(summary));
    snprintf(summary.dataset_group, sizeof(summary.dataset_group), "%s", group);
    snprintf(summary.dataset_file, sizeof(summary.dataset_file), "GROUP_AVERAGE");
    snprintf(summary.algorithm, sizeof(summary.algorithm), "%s", algorithm);
    summary.detection_interval = k;
    summary.deadlock_detected = 1;

    for (i = 0; i < results->count; ++i) {
        const ResultRow *row = &results->rows[i];
        if (!same_summary_key(row, group, algorithm, k)) continue;
        ++count;
        summary.runs += row->runs;
        summary.deadlock_detected = summary.deadlock_detected && row->deadlock_detected;
        summary.process_count += row->process_count;
        summary.resource_count += row->resource_count;
        summary.event_count += row->event_count;
        summary.detection_frequency += row->detection_frequency;
        summary.detection_time_seconds += row->detection_time_seconds;
        summary.detection_overhead_seconds += row->detection_overhead_seconds;
        summary.max_wait_for_edges += row->max_wait_for_edges;
        if (row->actual_deadlock_available) {
            summary.actual_deadlock_time += row->actual_deadlock_time;
            ++actual_count;
        }
        if (row->detected_deadlock_available) {
            summary.detected_deadlock_time += row->detected_deadlock_time;
            ++detected_count;
        }
        if (row->detection_latency_available) {
            summary.detection_latency += row->detection_latency;
            ++latency_count;
        }
    }

    if (count > 0) {
        summary.process_count /= count;
        summary.resource_count /= count;
        summary.event_count /= count;
        summary.detection_frequency /= count;
        summary.detection_time_seconds /= count;
        summary.detection_overhead_seconds /= count;
        summary.max_wait_for_edges /= count;
    }
    if (actual_count > 0) {
        summary.actual_deadlock_time /= actual_count;
        summary.actual_deadlock_available = 1;
    }
    if (detected_count > 0) {
        summary.detected_deadlock_time /= detected_count;
        summary.detected_deadlock_available = 1;
    }
    if (latency_count > 0) {
        summary.detection_latency /= latency_count;
        summary.detection_latency_available = 1;
    }
    return summary;
}

static int write_group_summary(const char *path, const ResultList *results, const Arguments *args) {
    ResultList summary = {0};
    int group_index;
    int algorithm_index;
    int k_index;
    int success;

    for (group_index = 0; group_index < 4; ++group_index) {
        const char *group = GROUP_NAMES[group_index];
        if (!group_allowed(args, group)) continue;
        for (algorithm_index = 0; algorithm_index < args->algorithm_count; ++algorithm_index) {
            const char *algorithm = algorithm_name(args->algorithms[algorithm_index]);
            for (k_index = 0; k_index < args->k_count; ++k_index) {
                ResultRow row = average_group_rows(results, group, algorithm, args->k_values[k_index]);
                if (row.runs > 0 && !add_result(&summary, &row)) {
                    free_result_list(&summary);
                    return 0;
                }
            }
        }
    }
    qsort(summary.rows, summary.count, sizeof(ResultRow), compare_result_rows);
    success = write_result_csv(path, &summary);
    free_result_list(&summary);
    return success;
}

static int write_config(const char *path, const Arguments *args, const StringList *dataset_files) {
    FILE *file;
    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);
    char timestamp[64] = "";
    if (time_info != NULL) strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", time_info);

    file = fopen(path, "w");
    if (file == NULL) return 0;
    fprintf(file, "{\n");
    fprintf(file, "  \"created_at\": \"%s\",\n", timestamp);
    fprintf(file, "  \"dataset_dir\": \"%s\",\n", args->dataset_dir);
    fprintf(file, "  \"output_dir\": \"%s\",\n", args->output_dir);
    fprintf(file, "  \"repeat\": %d,\n", args->repeat);
    fprintf(file, "  \"dataset_count\": %zu\n", dataset_files->count);
    fprintf(file, "}\n");
    fclose(file);
    return 1;
}

static int parse_arguments(int argc, char **argv, Arguments *args) {
    int i;
    args->dataset_dir = "datasets";
    args->output_dir = "results";
    args->algorithms[0] = ALGO_DFS;
    args->algorithms[1] = ALGO_BFS;
    args->algorithm_count = 2;
    args->k_values[0] = 1;
    args->k_values[1] = 2;
    args->k_values[2] = 5;
    args->k_values[3] = 10;
    args->k_count = 4;
    args->repeat = 5;
    args->group_count = 0;
    args->fail_fast = 0;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--dataset-dir") == 0) {
            if (++i >= argc) return 0;
            args->dataset_dir = argv[i];
        } else if (strcmp(argv[i], "--output-dir") == 0) {
            if (++i >= argc) return 0;
            args->output_dir = argv[i];
        } else if (strcmp(argv[i], "--repeat") == 0) {
            if (++i >= argc) return 0;
            args->repeat = atoi(argv[i]);
        } else if (strcmp(argv[i], "--fail-fast") == 0) {
            args->fail_fast = 1;
        } else if (strcmp(argv[i], "--algorithms") == 0) {
            int count = 0;
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                Algorithm algorithm;
                ++i;
                if (count >= MAX_ALGORITHMS || !parse_algorithm(argv[i], &algorithm)) return 0;
                args->algorithms[count++] = algorithm;
            }
            if (count == 0) return 0;
            args->algorithm_count = count;
        } else if (strcmp(argv[i], "--k") == 0) {
            int count = 0;
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                int value;
                ++i;
                value = atoi(argv[i]);
                if (count >= MAX_K_VALUES || value < 1) return 0;
                args->k_values[count++] = value;
            }
            if (count == 0) return 0;
            args->k_count = count;
        } else if (strcmp(argv[i], "--groups") == 0) {
            int count = 0;
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                ++i;
                if (count >= MAX_GROUP_FILTERS) return 0;
                args->groups[count++] = argv[i];
            }
            if (count == 0) return 0;
            args->group_count = count;
        } else {
            return 0;
        }
    }

    return args->repeat >= 1;
}

int main(int argc, char **argv) {
    Arguments args;
    StringList dataset_files = {0};
    ResultList results = {0};
    int error_count = 0;
    size_t file_index;
    char output_path[PATH_MAX];
    char summary_path[PATH_MAX];
    char config_path[PATH_MAX];

    if (!parse_arguments(argc, argv, &args) || !is_directory(args.dataset_dir)) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (!ensure_directory(args.output_dir)) {
        fprintf(stderr, "Cannot create output directory: %s\n", args.output_dir);
        return EXIT_FAILURE;
    }
    if (!scan_dataset_files(args.dataset_dir, args.dataset_dir, &args, &dataset_files)) {
        fprintf(stderr, "Failed to scan dataset directory: %s\n", args.dataset_dir);
        free_string_list(&dataset_files);
        return EXIT_FAILURE;
    }
    if (dataset_files.count == 0) {
        fprintf(stderr, "No dataset CSV files found in: %s\n", args.dataset_dir);
        free_string_list(&dataset_files);
        return EXIT_FAILURE;
    }
    qsort(dataset_files.items, dataset_files.count, sizeof(char *), compare_string_ptr);

    for (file_index = 0; file_index < dataset_files.count; ++file_index) {
        Dataset dataset;
        char error[512];
        int algorithm_index;
        int k_index;

        if (!read_dataset_csv(dataset_files.items[file_index], &dataset, error, sizeof(error))) {
            ++error_count;
            fprintf(stderr, "[ERROR] %s: %s\n", dataset_files.items[file_index], error);
            if (args.fail_fast) {
                free_string_list(&dataset_files);
                free_result_list(&results);
                return EXIT_FAILURE;
            }
            continue;
        }

        for (algorithm_index = 0; algorithm_index < args.algorithm_count; ++algorithm_index) {
            for (k_index = 0; k_index < args.k_count; ++k_index) {
                ResultRow row;
                if (!run_configuration(dataset_files.items[file_index], args.dataset_dir, &dataset,
                                       args.algorithms[algorithm_index], args.k_values[k_index],
                                       args.repeat, &row, error, sizeof(error)) ||
                    !add_result(&results, &row)) {
                    ++error_count;
                    fprintf(stderr, "[ERROR] %s: %s\n", dataset_files.items[file_index], error);
                    if (args.fail_fast) {
                        free_dataset(&dataset);
                        free_string_list(&dataset_files);
                        free_result_list(&results);
                        return EXIT_FAILURE;
                    }
                }
            }
        }
        free_dataset(&dataset);
    }

    qsort(results.rows, results.count, sizeof(ResultRow), compare_result_rows);
    snprintf(output_path, sizeof(output_path), "%s/experiment_results.csv", args.output_dir);
    snprintf(summary_path, sizeof(summary_path), "%s/group_summary.csv", args.output_dir);
    snprintf(config_path, sizeof(config_path), "%s/experiment_config.json", args.output_dir);

    if (!write_result_csv(output_path, &results) ||
        !write_group_summary(summary_path, &results, &args) ||
        !write_config(config_path, &args, &dataset_files)) {
        fprintf(stderr, "Failed to write output files. Check directory permissions.\n");
        free_string_list(&dataset_files);
        free_result_list(&results);
        return EXIT_FAILURE;
    }

    printf("Datasets scanned : %zu\n", dataset_files.count);
    printf("Result rows      : %zu\n", results.count);
    printf("Summary rows     : %d\n", args.group_count == 0 ? 4 * args.algorithm_count * args.k_count :
           args.group_count * args.algorithm_count * args.k_count);
    printf("Errors           : %d\n", error_count);
    printf("Output           : %s\n", output_path);
    printf("Group summary    : %s\n", summary_path);
    printf("Config           : %s\n", config_path);

    free_string_list(&dataset_files);
    free_result_list(&results);
    return error_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
