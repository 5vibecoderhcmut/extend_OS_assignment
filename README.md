# Deadlock Detection with Wait-for Graph

This project simulates deadlock detection using a Wait-for Graph and measures the cost of detection under different trigger intervals and system sizes.

## What the project does

- Reads lock/unlock event logs from CSV.
- Maintains lock ownership and waiting state.
- Builds a Wait-for Graph where each node is a process/thread.
- Adds an edge `Pi -> Pj` when `Pi` is waiting for a resource currently held by `Pj`.
- Detects deadlock by checking for cycles.
- Measures detection cost, latency, graph size, and simulation time.
- Generates plots and summary tables for report writing.

## Repo layout

- `deadlock_simulator.py`: core simulator and deadlock detector.
- `generate_dataset.py`: synthetic dataset generator for large-scale experiments.
- `run_experiments.py`: batch experiments and plot generation.
- `datasets/`: provided sample datasets.
- `results/`: generated plots, summary CSV, and synthetic datasets for experiments.

## Input format

The simulator supports the repo CSV format:

```csv
timestamp,thread_id,thread_type,action,target_resource,resource_type,duration_ms,timeout_threshold
0,P1,UI,LOCK,R1,Mutex,null,5000
10,P2,WORKER,LOCK,R2,Mutex,null,null
```

It also accepts the simpler spec format:

```csv
time,process_id,action,resource_id
```

Supported actions:

- `LOCK`, `REQUEST`, `REQ`, `ACQUIRE`
- `UNLOCK`, `RELEASE`, `REL`

## Core idea

If process `Pi` requests a resource held by `Pj`, the simulator adds a directed edge:

```text
Pi -> Pj
```

A deadlock exists when the Wait-for Graph contains a cycle.

## How to run

### 1. Run a single dataset

Detect every event:

```bash
python3 deadlock_simulator.py datasets/case_1.csv -k 1 --algorithm dfs
```

Detect every 10 events:

```bash
python3 deadlock_simulator.py datasets/case_20.csv -k 10 --algorithm dfs
```

Compare DFS and BFS:

```bash
python3 deadlock_simulator.py datasets/case_20.csv -k 10 --algorithm bfs
```

### 2. Generate a larger dataset

```bash
python3 generate_dataset.py datasets/generated_large.csv --processes 50 --resources 60 --events 5000
```

### 3. Run the full experiment suite

```bash
python3 run_experiments.py
```

This creates:

- `results/frequency_tradeoff.png`
- `results/overhead_latency_tradeoff.png`
- `results/dfs_vs_bfs.png`
- `results/graph_build_time.png`
- `results/scalability.png`
- `results/scale_by_k_matrix.png`
- `results/experiment_summary.csv`

## Experiments performed

### Frequency experiment

Tests one dataset with many trigger intervals:

- `K = 1, 2, 5, 10, 20, 50, 100, 200, 500`

This shows the trade-off between:

- detection frequency
- detection overhead
- detection latency

### Scalability experiment

Tests larger synthetic datasets with increasing size:

- `5 processes / 8 resources / 200 events`
- `10 processes / 15 resources / 500 events`
- `20 processes / 30 resources / 1000 events`
- `50 processes / 60 resources / 3000 events`
- `100 processes / 120 resources / 6000 events`
- `150 processes / 180 resources / 9000 events`
- `200 processes / 240 resources / 12000 events`

### Scale x K experiment

Combines larger datasets with many `K` values to show how scale and trigger interval interact.

## Metrics

- `avg_detection_time_seconds`: average time spent per detection call.
- `total_detection_time_seconds`: total time spent inside the detection algorithm.
- `avg_graph_build_time_seconds`: average time to build the Wait-for Graph per event.
- `total_graph_build_time_seconds`: total time spent building graph snapshots.
- `total_simulation_time_seconds`: total runtime of the simulation.
- `detection_frequency`: number of detection calls.
- `detection_latency`: difference between first real deadlock time and first detected deadlock time.
- `detection_overhead`: `avg_detection_time_seconds * detection_frequency`.
- `cycle_length`: number of process-to-process hops in the detected cycle.
- `avg_wait_for_nodes`: average number of nodes in the Wait-for Graph.
- `avg_wait_for_edges`: average number of edges in the Wait-for Graph.
- `max_wait_for_nodes`: maximum number of nodes observed.
- `max_wait_for_edges`: maximum number of edges observed.

## How to read the plots

- `frequency_tradeoff.png`: higher `K` usually means lower overhead but higher latency.
- `overhead_latency_tradeoff.png`: shows the direct trade-off between cost and delay.
- `dfs_vs_bfs.png`: compares average detection time of DFS and BFS.
- `graph_build_time.png`: shows the cost of building the Wait-for Graph.
- `scalability.png`: shows how detection cost grows with system size.
- `scale_by_k_matrix.png`: heatmap of latency and overhead across both scale and `K`.

## Report-ready observations

- Smaller `K` detects deadlocks sooner, but it calls the detector more often.
- Larger `K` reduces overhead, but deadlock can remain undiscovered for a long time.
- As process/resource count grows, the Wait-for Graph becomes larger and detection cost increases.
- Graph construction cost is also meaningful, not just cycle detection cost.
- DFS is the main detector in this project; BFS is included for comparison.

## Notes

- The repository already includes sample datasets in `datasets/`.
- Synthetic experiment datasets and outputs are written under `results/`.
- If you rerun experiments, the generated CSV and plots will be overwritten.
