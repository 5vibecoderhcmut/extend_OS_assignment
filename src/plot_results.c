#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_FIELDS 20
#define SVG_WIDTH 1000
#define SVG_HEIGHT 620
#define LEFT_MARGIN 100
#define RIGHT_MARGIN 240
#define TOP_MARGIN 80
#define BOTTOM_MARGIN 100

typedef struct {
    char group[32];
    char file[PATH_MAX];
    char algorithm[8];
    int k;
    double process_count;
    double resource_count;
    double event_count;
    int runs;
    double actual_time;
    int has_actual_time;
    double detected_time;
    int has_detected_time;
    double latency;
    int has_latency;
    double frequency;
    double detection_time;
    double overhead;
    double max_edges;
} PlotRow;

typedef struct {
    PlotRow *rows;
    size_t count;
    size_t capacity;
} PlotRows;

typedef struct {
    const char *name;
    const char *color;
} GroupStyle;

static const GroupStyle GROUP_STYLES[] = {
    {"small", "#2E86AB"},
    {"medium", "#F18F01"},
    {"large", "#A23B72"},
    {"very_large", "#2A9D8F"}
};

static const char *DFS_COLOR = "#4C78A8";
static const char *BFS_COLOR = "#F58518";

static void usage(const char *program) {
    fprintf(stderr,
            "Usage: %s [--results-dir results] [--output-dir plots] "
            "[--primary-algorithm dfs] [--scale-k 1]\n",
            program);
}

static int ensure_directory(const char *path) {
    char buffer[PATH_MAX];
    char *cursor;
    size_t length;
    if (path == NULL || path[0] == '\0' || strlen(path) >= sizeof(buffer)) return 0;
    strcpy(buffer, path);
    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '/') buffer[length - 1] = '\0';
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

static char *trim(char *text) {
    char *end;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') ++text;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) --end;
    *end = '\0';
    return text;
}

static int split_csv_simple(char *line, char **fields, int max_fields) {
    int count = 0;
    char *cursor = line;
    if (max_fields <= 0) return 0;
    fields[count++] = cursor;
    while (*cursor != '\0') {
        if (*cursor == ',') {
            *cursor = '\0';
            if (count < max_fields) fields[count++] = cursor + 1;
        }
        ++cursor;
    }
    while (count < max_fields) fields[count++] = (char *)"";
    return count;
}

static double parse_double(const char *text, int *available) {
    char *endptr;
    double value;
    if (text == NULL || *trim((char *)text) == '\0') {
        *available = 0;
        return 0.0;
    }
    value = strtod(text, &endptr);
    if (*trim(endptr) != '\0') {
        *available = 0;
        return 0.0;
    }
    *available = 1;
    return value;
}

static int add_plot_row(PlotRows *rows, const PlotRow *row) {
    PlotRow *new_rows;
    if (rows->count == rows->capacity) {
        size_t new_capacity = rows->capacity == 0 ? 64 : rows->capacity * 2;
        new_rows = realloc(rows->rows, new_capacity * sizeof(PlotRow));
        if (new_rows == NULL) return 0;
        rows->rows = new_rows;
        rows->capacity = new_capacity;
    }
    rows->rows[rows->count++] = *row;
    return 1;
}

static void free_plot_rows(PlotRows *rows) {
    free(rows->rows);
    memset(rows, 0, sizeof(*rows));
}

static int read_csv_rows(const char *path, PlotRows *rows, int required) {
    FILE *file = fopen(path, "r");
    char line[8192];
    int line_number = 0;
    if (file == NULL) {
        if (required) fprintf(stderr, "Cannot open result file: %s\n", path);
        return required ? 0 : 1;
    }
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        fprintf(stderr, "Result file is empty: %s\n", path);
        return 0;
    }
    ++line_number;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *fields[MAX_FIELDS];
        PlotRow row;
        int available;
        ++line_number;
        split_csv_simple(line, fields, MAX_FIELDS);
        memset(&row, 0, sizeof(row));
        snprintf(row.group, sizeof(row.group), "%s", trim(fields[0]));
        snprintf(row.file, sizeof(row.file), "%s", trim(fields[1]));
        row.process_count = parse_double(trim(fields[2]), &available);
        row.resource_count = parse_double(trim(fields[3]), &available);
        row.event_count = parse_double(trim(fields[4]), &available);
        snprintf(row.algorithm, sizeof(row.algorithm), "%s", trim(fields[5]));
        row.k = atoi(trim(fields[6]));
        row.runs = atoi(trim(fields[7]));
        row.actual_time = parse_double(trim(fields[9]), &row.has_actual_time);
        row.detected_time = parse_double(trim(fields[10]), &row.has_detected_time);
        row.latency = parse_double(trim(fields[11]), &row.has_latency);
        row.frequency = parse_double(trim(fields[12]), &available);
        row.detection_time = parse_double(trim(fields[13]), &available);
        row.overhead = parse_double(trim(fields[14]), &available);
        row.max_edges = parse_double(trim(fields[15]), &available);
        if (row.group[0] == '\0' || row.algorithm[0] == '\0' || row.k <= 0) {
            fprintf(stderr, "Skipping malformed result row %d in %s\n", line_number, path);
            continue;
        }
        if (!add_plot_row(rows, &row)) {
            fclose(file);
            fprintf(stderr, "Out of memory while reading %s\n", path);
            return 0;
        }
    }
    fclose(file);
    return 1;
}

static int group_index(const char *group) {
    size_t i;
    for (i = 0; i < sizeof(GROUP_STYLES) / sizeof(GROUP_STYLES[0]); ++i) {
        if (strcmp(group, GROUP_STYLES[i].name) == 0) return (int)i;
    }
    return -1;
}

static const char *group_color(const char *group) {
    int index = group_index(group);
    return index >= 0 ? GROUP_STYLES[index].color : "#555555";
}

static int group_present(const PlotRows *rows, const char *group) {
    size_t i;
    for (i = 0; i < rows->count; ++i) {
        if (strcmp(rows->rows[i].group, group) == 0) return 1;
    }
    return 0;
}

static void svg_begin(FILE *file, const char *title) {
    fprintf(file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(file, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n",
            SVG_WIDTH, SVG_HEIGHT, SVG_WIDTH, SVG_HEIGHT);
    fprintf(file, "<rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n");
    fprintf(file, "<text x=\"%d\" y=\"38\" font-family=\"Arial\" font-size=\"24\" font-weight=\"bold\" fill=\"#222\">%s</text>\n",
            LEFT_MARGIN, title);
}

static void svg_end(FILE *file) {
    fprintf(file, "</svg>\n");
}

static void svg_text(FILE *file, double x, double y, const char *text, int size, const char *anchor) {
    fprintf(file, "<text x=\"%.2f\" y=\"%.2f\" text-anchor=\"%s\" font-family=\"Arial\" font-size=\"%d\" fill=\"#222\">%s</text>\n",
            x, y, anchor, size, text);
}

static void format_value(double value, char *buffer, size_t size) {
    if (fabs(value) > 0 && (fabs(value) < 0.001 || fabs(value) >= 10000.0)) {
        snprintf(buffer, size, "%.1e", value);
    } else if (fabs(value) < 10.0) {
        snprintf(buffer, size, "%.3f", value);
    } else {
        snprintf(buffer, size, "%.1f", value);
    }
}

static void svg_axes(FILE *file, double y_max, const char *y_label, const char *x_label) {
    const double x0 = LEFT_MARGIN;
    const double y0 = SVG_HEIGHT - BOTTOM_MARGIN;
    const double x1 = SVG_WIDTH - RIGHT_MARGIN;
    const double y1 = TOP_MARGIN;
    int tick;
    fprintf(file, "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"#222\" stroke-width=\"1.5\"/>\n", x0, y0, x1, y0);
    fprintf(file, "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"#222\" stroke-width=\"1.5\"/>\n", x0, y0, x0, y1);
    for (tick = 0; tick <= 5; ++tick) {
        double fraction = (double)tick / 5.0;
        double y = y0 - fraction * (y0 - y1);
        char label[64];
        format_value(y_max * fraction, label, sizeof(label));
        fprintf(file, "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"#D9D9D9\" stroke-width=\"1\"/>\n", x0, y, x1, y);
        svg_text(file, x0 - 10, y + 4, label, 12, "end");
    }
    svg_text(file, (x0 + x1) / 2.0, SVG_HEIGHT - 25, x_label, 16, "middle");
    fprintf(file, "<text x=\"25\" y=\"%.2f\" transform=\"rotate(-90 25 %.2f)\" text-anchor=\"middle\" font-family=\"Arial\" font-size=\"16\" fill=\"#222\">%s</text>\n",
            (y0 + y1) / 2.0, (y0 + y1) / 2.0, y_label);
}

static double y_to_svg(double value, double y_max) {
    const double y0 = SVG_HEIGHT - BOTTOM_MARGIN;
    const double y1 = TOP_MARGIN;
    if (y_max <= 0.0) return y0;
    return y0 - (value / y_max) * (y0 - y1);
}

static double x_to_svg(int position, int count) {
    const double x0 = LEFT_MARGIN;
    const double x1 = SVG_WIDTH - RIGHT_MARGIN;
    if (count <= 1) return (x0 + x1) / 2.0;
    return x0 + (double)position * (x1 - x0) / (double)(count - 1);
}

static int metric_value(const PlotRow *row, const char *metric, double *value) {
    if (strcmp(metric, "latency") == 0) {
        if (!row->has_latency) return 0;
        *value = row->latency;
    } else if (strcmp(metric, "overhead") == 0) {
        *value = row->overhead;
    } else if (strcmp(metric, "frequency") == 0) {
        *value = row->frequency;
    } else if (strcmp(metric, "detection_time") == 0) {
        *value = row->detection_time;
    } else if (strcmp(metric, "max_edges") == 0) {
        *value = row->max_edges;
    } else {
        return 0;
    }
    return 1;
}

static const PlotRow *find_row(const PlotRows *rows, const char *group, const char *algorithm, int k) {
    size_t i;
    for (i = 0; i < rows->count; ++i) {
        const PlotRow *row = &rows->rows[i];
        if (strcmp(row->group, group) == 0 && strcmp(row->algorithm, algorithm) == 0 && row->k == k) {
            return row;
        }
    }
    return NULL;
}

static int collect_k_values(const PlotRows *rows, const char *algorithm, int *values, int max_values) {
    int count = 0;
    size_t i;
    for (i = 0; i < rows->count; ++i) {
        int seen = 0;
        int j;
        if (strcmp(rows->rows[i].algorithm, algorithm) != 0) continue;
        for (j = 0; j < count; ++j) if (values[j] == rows->rows[i].k) seen = 1;
        if (!seen && count < max_values) values[count++] = rows->rows[i].k;
    }
    for (i = 0; i < (size_t)count; ++i) {
        int j;
        for (j = (int)i + 1; j < count; ++j) {
            if (values[j] < values[i]) {
                int temp = values[i]; values[i] = values[j]; values[j] = temp;
            }
        }
    }
    return count;
}

static int line_chart_by_group(const PlotRows *rows,
                               const char *output_dir,
                               const char *algorithm,
                               const char *metric,
                               const char *title,
                               const char *ylabel,
                               const char *filename) {
    char path[PATH_MAX];
    FILE *file;
    int k_values[32];
    int k_count = collect_k_values(rows, algorithm, k_values, 32);
    double y_max = 0.0;
    int group_i;
    int k_i;

    if (k_count == 0) return 0;
    for (group_i = 0; group_i < 4; ++group_i) {
        const char *group = GROUP_STYLES[group_i].name;
        if (!group_present(rows, group)) continue;
        for (k_i = 0; k_i < k_count; ++k_i) {
            const PlotRow *row = find_row(rows, group, algorithm, k_values[k_i]);
            double value;
            if (row != NULL && metric_value(row, metric, &value) && value > y_max) y_max = value;
        }
    }
    if (y_max <= 0.0) y_max = 1.0;
    y_max *= 1.08;

    snprintf(path, sizeof(path), "%s/%s.svg", output_dir, filename);
    file = fopen(path, "w");
    if (file == NULL) return 0;
    svg_begin(file, title);
    svg_axes(file, y_max, ylabel, "Detection interval K");

    for (k_i = 0; k_i < k_count; ++k_i) {
        char label[32];
        double x = x_to_svg(k_i, k_count);
        snprintf(label, sizeof(label), "%d", k_values[k_i]);
        fprintf(file, "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"#222\" stroke-width=\"1\"/>\n",
                x, (double)(SVG_HEIGHT - BOTTOM_MARGIN), x,
                (double)(SVG_HEIGHT - BOTTOM_MARGIN + 6));
        svg_text(file, x, SVG_HEIGHT - BOTTOM_MARGIN + 25, label, 13, "middle");
    }

    for (group_i = 0; group_i < 4; ++group_i) {
        const char *group = GROUP_STYLES[group_i].name;
        const char *color = GROUP_STYLES[group_i].color;
        int first = 1;
        double previous_x = 0.0;
        double previous_y = 0.0;
        int legend_y;
        if (!group_present(rows, group)) continue;
        for (k_i = 0; k_i < k_count; ++k_i) {
            const PlotRow *row = find_row(rows, group, algorithm, k_values[k_i]);
            double value;
            double x;
            double y;
            if (row == NULL || !metric_value(row, metric, &value)) continue;
            x = x_to_svg(k_i, k_count);
            y = y_to_svg(value, y_max);
            if (!first) {
                fprintf(file, "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"%s\" stroke-width=\"3\"/>\n",
                        previous_x, previous_y, x, y, color);
            }
            fprintf(file, "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"5\" fill=\"%s\"/>\n", x, y, color);
            previous_x = x;
            previous_y = y;
            first = 0;
        }
        legend_y = TOP_MARGIN + group_i * 28;
        fprintf(file, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"%s\" stroke-width=\"3\"/>\n",
                SVG_WIDTH - RIGHT_MARGIN + 35, legend_y - 5, SVG_WIDTH - RIGHT_MARGIN + 60, legend_y - 5, color);
        svg_text(file, SVG_WIDTH - RIGHT_MARGIN + 68, legend_y, group, 13, "start");
    }

    svg_text(file, SVG_WIDTH - RIGHT_MARGIN + 35, TOP_MARGIN - 25, "Dataset group", 14, "start");
    svg_end(file);
    fclose(file);
    return 1;
}

static int bar_chart_by_group(const PlotRows *rows,
                              const char *output_dir,
                              int k_value,
                              const char *metric,
                              const char *title,
                              const char *ylabel,
                              const char *filename) {
    char path[PATH_MAX];
    FILE *file;
    const char *algorithms[] = {"dfs", "bfs"};
    const char *colors[] = {NULL, NULL};
    int groups[4];
    int group_count = 0;
    int g;
    int a;
    double y_max = 0.0;

    colors[0] = DFS_COLOR;
    colors[1] = BFS_COLOR;
    for (g = 0; g < 4; ++g) {
        if (group_present(rows, GROUP_STYLES[g].name)) groups[group_count++] = g;
    }
    if (group_count == 0) return 0;

    for (g = 0; g < group_count; ++g) {
        for (a = 0; a < 2; ++a) {
            const PlotRow *row = find_row(rows, GROUP_STYLES[groups[g]].name, algorithms[a], k_value);
            double value;
            if (row != NULL && metric_value(row, metric, &value) && value > y_max) y_max = value;
        }
    }
    if (y_max <= 0.0) y_max = 1.0;
    y_max *= 1.08;

    snprintf(path, sizeof(path), "%s/%s.svg", output_dir, filename);
    file = fopen(path, "w");
    if (file == NULL) return 0;
    svg_begin(file, title);
    svg_axes(file, y_max, ylabel, "Dataset group");

    for (g = 0; g < group_count; ++g) {
        double center = x_to_svg(g, group_count);
        double group_width = 100.0;
        svg_text(file, center, SVG_HEIGHT - BOTTOM_MARGIN + 28, GROUP_STYLES[groups[g]].name, 13, "middle");
        for (a = 0; a < 2; ++a) {
            const PlotRow *row = find_row(rows, GROUP_STYLES[groups[g]].name, algorithms[a], k_value);
            double value = 0.0;
            double bar_width = group_width / 2.6;
            double x;
            double y;
            double height;
            if (row != NULL) metric_value(row, metric, &value);
            x = center - group_width / 2.0 + a * (bar_width + 8.0);
            y = y_to_svg(value, y_max);
            height = SVG_HEIGHT - BOTTOM_MARGIN - y;
            fprintf(file, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" fill=\"%s\"/>\n",
                    x, y, bar_width, height, colors[a]);
        }
    }

    for (a = 0; a < 2; ++a) {
        int y = TOP_MARGIN + a * 28;
        fprintf(file, "<rect x=\"%d\" y=\"%d\" width=\"18\" height=\"18\" fill=\"%s\"/>\n",
                SVG_WIDTH - RIGHT_MARGIN + 35, y - 15, colors[a]);
        svg_text(file, SVG_WIDTH - RIGHT_MARGIN + 62, y, algorithms[a], 13, "start");
    }
    svg_text(file, SVG_WIDTH - RIGHT_MARGIN + 35, TOP_MARGIN - 25, "Algorithm", 14, "start");
    {
        char k_label[32];
        snprintf(k_label, sizeof(k_label), "K = %d", k_value);
        svg_text(file, SVG_WIDTH - RIGHT_MARGIN + 35, SVG_HEIGHT - BOTTOM_MARGIN + 62, k_label, 13, "start");
    }
    svg_end(file);
    fclose(file);
    return 1;
}

static int scatter_detection_time_vs_edges(const PlotRows *rows,
                                           const char *output_dir,
                                           int k_value) {
    char path[PATH_MAX];
    FILE *file;
    size_t i;
    double x_max = 0.0;
    double y_max = 0.0;
    int found = 0;
    const double x0 = LEFT_MARGIN;
    const double x1 = SVG_WIDTH - RIGHT_MARGIN;
    const double y0 = SVG_HEIGHT - BOTTOM_MARGIN;
    const double y1 = TOP_MARGIN;

    for (i = 0; i < rows->count; ++i) {
        if (rows->rows[i].k != k_value) continue;
        if (rows->rows[i].max_edges > x_max) x_max = rows->rows[i].max_edges;
        if (rows->rows[i].detection_time > y_max) y_max = rows->rows[i].detection_time;
        found = 1;
    }
    if (!found) return 0;
    if (x_max <= 0.0) x_max = 1.0;
    if (y_max <= 0.0) y_max = 1.0;
    x_max *= 1.08;
    y_max *= 1.08;

    snprintf(path, sizeof(path), "%s/detection_time_vs_edges.svg", output_dir);
    file = fopen(path, "w");
    if (file == NULL) return 0;
    {
        char title[128];
        snprintf(title, sizeof(title), "Detection time vs Wait-for Graph edges (K=%d)", k_value);
        svg_begin(file, title);
    }
    svg_axes(file, y_max, "Average detection time per call (seconds)", "Maximum number of WFG edges");

    for (i = 0; i < rows->count; ++i) {
        const PlotRow *row = &rows->rows[i];
        double x;
        double y;
        const char *color;
        if (row->k != k_value) continue;
        x = x0 + (row->max_edges / x_max) * (x1 - x0);
        y = y0 - (row->detection_time / y_max) * (y0 - y1);
        color = group_color(row->group);
        if (strcmp(row->algorithm, "bfs") == 0) {
            fprintf(file, "<rect x=\"%.2f\" y=\"%.2f\" width=\"9\" height=\"9\" fill=\"%s\"/>\n", x - 4.5, y - 4.5, color);
        } else {
            fprintf(file, "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"5\" fill=\"%s\"/>\n", x, y, color);
        }
    }

    /* X-axis numeric ticks. */
    for (int tick = 0; tick <= 5; ++tick) {
        double fraction = (double)tick / 5.0;
        double x = x0 + fraction * (x1 - x0);
        char label[64];
        format_value(x_max * fraction, label, sizeof(label));
        fprintf(file, "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"#222\" stroke-width=\"1\"/>\n", x, y0, x, y0 + 6);
        svg_text(file, x, y0 + 25, label, 12, "middle");
    }

    for (int g = 0; g < 4; ++g) {
        int y = TOP_MARGIN + g * 26;
        fprintf(file, "<circle cx=\"%d\" cy=\"%d\" r=\"5\" fill=\"%s\"/>\n",
                SVG_WIDTH - RIGHT_MARGIN + 40, y - 5, GROUP_STYLES[g].color);
        svg_text(file, SVG_WIDTH - RIGHT_MARGIN + 52, y, GROUP_STYLES[g].name, 13, "start");
    }
    svg_text(file, SVG_WIDTH - RIGHT_MARGIN + 35, TOP_MARGIN - 25, "Color = group", 14, "start");
    svg_text(file, SVG_WIDTH - RIGHT_MARGIN + 35, TOP_MARGIN + 125, "Circle = DFS", 12, "start");
    svg_text(file, SVG_WIDTH - RIGHT_MARGIN + 35, TOP_MARGIN + 145, "Square = BFS", 12, "start");
    svg_end(file);
    fclose(file);
    return 1;
}

int main(int argc, char **argv) {
    const char *results_dir = "results";
    const char *output_dir = "plots";
    const char *primary_algorithm = "dfs";
    int scale_k = 1;
    int i;
    char summary_path[PATH_MAX];
    char result_path[PATH_MAX];
    PlotRows summary_rows = {0};
    PlotRows experiment_rows = {0};
    int created = 0;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--results-dir") == 0) {
            if (++i >= argc) { usage(argv[0]); return EXIT_FAILURE; }
            results_dir = argv[i];
        } else if (strcmp(argv[i], "--output-dir") == 0) {
            if (++i >= argc) { usage(argv[0]); return EXIT_FAILURE; }
            output_dir = argv[i];
        } else if (strcmp(argv[i], "--primary-algorithm") == 0) {
            if (++i >= argc) { usage(argv[0]); return EXIT_FAILURE; }
            primary_algorithm = argv[i];
        } else if (strcmp(argv[i], "--scale-k") == 0) {
            if (++i >= argc) { usage(argv[0]); return EXIT_FAILURE; }
            scale_k = atoi(argv[i]);
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!ensure_directory(output_dir)) {
        fprintf(stderr, "Cannot create plot directory: %s\n", output_dir);
        return EXIT_FAILURE;
    }
    snprintf(summary_path, sizeof(summary_path), "%s/group_summary.csv", results_dir);
    snprintf(result_path, sizeof(result_path), "%s/experiment_results.csv", results_dir);
    if (!read_csv_rows(summary_path, &summary_rows, 1) ||
        !read_csv_rows(result_path, &experiment_rows, 0)) {
        free_plot_rows(&summary_rows);
        free_plot_rows(&experiment_rows);
        return EXIT_FAILURE;
    }

    created += line_chart_by_group(&summary_rows, output_dir, primary_algorithm, "latency",
                                   "Detection latency vs K by group", "Average latency (time units)",
                                   "latency_vs_k_by_group");
    created += line_chart_by_group(&summary_rows, output_dir, primary_algorithm, "overhead",
                                   "Detection overhead vs K by group", "Average total detection overhead (seconds)",
                                   "overhead_vs_k_by_group");
    created += line_chart_by_group(&summary_rows, output_dir, primary_algorithm, "frequency",
                                   "Detection frequency vs K by group", "Average number of detection calls",
                                   "frequency_vs_k_by_group");
    created += bar_chart_by_group(&summary_rows, output_dir, scale_k, "detection_time",
                                  "Average detection time by group", "Average detection time per call (seconds)",
                                  "detection_time_by_group_dfs_bfs");
    created += bar_chart_by_group(&summary_rows, output_dir, scale_k, "overhead",
                                  "Detection overhead by group", "Average total detection overhead (seconds)",
                                  "overhead_by_group_dfs_bfs");
    if (experiment_rows.count > 0) {
        created += scatter_detection_time_vs_edges(&experiment_rows, output_dir, scale_k);
    }

    free_plot_rows(&summary_rows);
    free_plot_rows(&experiment_rows);

    if (created == 0) {
        fprintf(stderr, "No plots were created. Run make all first.\n");
        return EXIT_FAILURE;
    }
    printf("Charts created: %d\n", created);
    printf("Plot format: SVG (no Python or external plotting library required).\n");
    return EXIT_SUCCESS;
}
