#include "deadlock_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program) {
    fprintf(stderr,
            "Usage: %s <dataset.csv> [-k K] [--algorithm dfs|bfs] [--trace]\n"
            "Example: %s datasets/small/small_random_5p.csv -k 5 --algorithm dfs --trace\n",
            program, program);
}

int main(int argc, char **argv) {
    const char *dataset_path = NULL;
    int detection_interval = 1;
    Algorithm algorithm = ALGO_DFS;
    int trace = 0;
    int i;
    Dataset dataset;
    SimulationResult result;
    char error[512];

    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--detection-interval") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
            detection_interval = atoi(argv[i]);
        } else if (strcmp(argv[i], "--algorithm") == 0) {
            if (++i >= argc || !parse_algorithm(argv[i], &algorithm)) {
                fprintf(stderr, "Unknown algorithm. Use dfs or bfs.\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--trace") == 0) {
            trace = 1;
        } else if (argv[i][0] == '-') {
            usage(argv[0]);
            return EXIT_FAILURE;
        } else if (dataset_path == NULL) {
            dataset_path = argv[i];
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (dataset_path == NULL || detection_interval < 1) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (!read_dataset_csv(dataset_path, &dataset, error, sizeof(error))) {
        fprintf(stderr, "Error: %s\n", error);
        return EXIT_FAILURE;
    }
    if (!simulate_dataset(&dataset, detection_interval, algorithm, trace,
                          &result, error, sizeof(error))) {
        fprintf(stderr, "Error: %s\n", error);
        free_dataset(&dataset);
        return EXIT_FAILURE;
    }

    print_simulation_result(&dataset, &result);
    free_dataset(&dataset);
    return EXIT_SUCCESS;
}
