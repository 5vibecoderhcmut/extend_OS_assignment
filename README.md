# Topic 3: Deadlock Detection using Wait-for Graph (C Version)

This project is a full C implementation of the Topic 3 deadlock detection simulator. It reads the provided CSV datasets, builds a Wait-for Graph after each event, detects cycles with DFS or BFS/Kahn's algorithm, and exports experiment results as CSV files.

No Python runtime, `pandas`, or `matplotlib` is required. The plotting utility is also written in C and produces SVG figures.

## Source layout

```text
src/
├── deadlock_core.h        # shared data structures and public functions
├── deadlock_core.c        # CSV reader, resource simulator, WFG builder, DFS and BFS detection
├── deadlock_simulator.c   # single-dataset CLI for tracing one simulation
├── run_experiments.c      # batch experiment runner and group-level summaries
└── plot_results.c         # C-based SVG chart generator
```

## Build requirements

The project targets Linux/WSL and requires only:

```bash
sudo apt update
sudo apt install build-essential make
```

## Build and run experiments

```bash
make all
```

or:

```bash
./run.sh
```

This command compiles the C programs and runs every CSV dataset with:

```text
Algorithms : DFS and BFS
K values   : 1, 2, 5, 10
Repeats    : 5 timing runs per configuration
```

The five repeated runs are averaged internally. The CSV output contains one average row per:

```text
1 dataset × 1 algorithm × 1 detection interval K
```

## Run one dataset with a trace

```bash
make simulator
./bin/deadlock_simulator datasets/small/small_random_5p.csv -k 5 --algorithm dfs --trace
```

The trace shows resource ownership and the current Wait-for Graph after each event.

## Result files

```text
results/
├── experiment_results.csv
├── group_summary.csv
└── experiment_config.json
```

`experiment_results.csv` contains one row for each dataset, algorithm, and K value.

`group_summary.csv` has the same columns, but each numeric metric is averaged across the five datasets in the same scale group:

```text
small, medium, large, very_large
```

The output columns are:

```text
dataset_group
dataset_file
process_count
resource_count
event_count
algorithm
detection_interval
runs
deadlock_detected
actual_deadlock_time
detected_deadlock_time
detection_latency
detection_frequency
detection_time_seconds
detection_overhead_seconds
max_wait_for_edges
```

## Generate group-level plots

```bash
make plot
```

The C plotting program reads `group_summary.csv` and `experiment_results.csv` and writes SVG figures to `plots/`:

```text
plots/
├── latency_vs_k_by_group.svg
├── overhead_vs_k_by_group.svg
├── frequency_vs_k_by_group.svg
├── detection_time_by_group_dfs_bfs.svg
├── overhead_by_group_dfs_bfs.svg
└── detection_time_vs_edges.svg
```

SVG files can be opened directly in a browser or used as screenshots for the experimental-results section of the report.

## Useful commands

```bash
make small
make medium
make large
make very_large
make clean
```
