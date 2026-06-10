# Deadlock Detection Using Wait-for Graph

## 1. Introduction

Deadlock is a classic problem in operating systems where a set of processes are permanently blocked because each process is waiting for a resource held by another process in the same set. This project implements deadlock detection using a Wait-for Graph and evaluates the trade-off between detection cost and detection latency.

## 2. Problem Statement

Given a stream of lock and unlock events, the system must:

- maintain resource ownership
- build a Wait-for Graph
- detect cycles in the graph
- measure detection cost under different trigger intervals
- study how the cost changes when the system scales

The assignment also asks for analysis on detection frequency, latency, and overhead.

## 3. System Model

Each process/thread is modeled as a node in the Wait-for Graph.

If process `Pi` is waiting for a resource currently owned by `Pj`, the graph contains a directed edge:

```text
Pi -> Pj
```

A deadlock exists if the graph contains at least one cycle.

## 4. Implementation

### 4.1 Core files

- [`deadlock_simulator.py`](./deadlock_simulator.py): main simulator and cycle detection.
- [`generate_dataset.py`](./generate_dataset.py): synthetic dataset generator.
- [`run_experiments.py`](./run_experiments.py): batch experiments and plot generation.

### 4.2 Detection algorithms

- DFS-based cycle detection is the main method.
- BFS/Kahn-style pruning is included for comparison.

### 4.3 Trigger modes

Detection is triggered:

- after every event (`K=1`)
- or every `K` events

This lets us measure the trade-off between responsiveness and overhead.

## 5. Metrics

The project reports:

- `avg_detection_time_seconds`
- `total_detection_time_seconds`
- `avg_graph_build_time_seconds`
- `total_graph_build_time_seconds`
- `total_simulation_time_seconds`
- `detection_frequency`
- `detection_latency`
- `detection_overhead`
- `cycle_length`
- `avg_wait_for_nodes`
- `avg_wait_for_edges`
- `max_wait_for_nodes`
- `max_wait_for_edges`

## 6. Experimental Setup

### 6.1 Datasets

The repository includes sample datasets in [`datasets/`](./datasets/).

For scalability testing, larger synthetic datasets are generated with:

- 5 to 200 processes
- 8 to 240 resources
- 200 to 12000 events

### 6.2 Trigger intervals

The main experiment uses:

```text
K = 1, 2, 5, 10, 20, 50, 100, 200, 500
```

### 6.3 Outputs

Running `python3 run_experiments.py` generates:

- [`results/frequency_tradeoff.png`](./results/frequency_tradeoff.png)
- [`results/overhead_latency_tradeoff.png`](./results/overhead_latency_tradeoff.png)
- [`results/dfs_vs_bfs.png`](./results/dfs_vs_bfs.png)
- [`results/graph_build_time.png`](./results/graph_build_time.png)
- [`results/scalability.png`](./results/scalability.png)
- [`results/scale_by_k_matrix.png`](./results/scale_by_k_matrix.png)
- [`results/experiment_summary.csv`](./results/experiment_summary.csv)

## 7. Results

### 7.1 Effect of K

On the 5000-event dataset:

| K | Detection Frequency | Overhead (s) | Latency |
|---|---:|---:|---:|
| 1 | 5000 | 0.040085 | 0 |
| 10 | 500 | 0.007818 | 0 |
| 20 | 250 | 0.001852 | 3206 |
| 50 | 100 | 0.000734 | 3515 |
| 100 | 50 | 0.000364 | 6028 |
| 500 | 10 | 0.000084 | 10284 |

Observation:

- larger `K` reduces overhead strongly
- larger `K` increases deadlock detection latency

### 7.2 Scalability

With `K=10`, DFS on larger datasets shows rising detection and simulation cost:

| Processes | Events | Avg Detection Time (s) | Total Simulation Time (s) |
|---|---:|---:|---:|
| 5 | 200 | 0.00000171 | 0.0007 |
| 50 | 3000 | 0.00000844 | 0.0847 |
| 100 | 6000 | 0.00001528 | 0.2954 |
| 200 | 12000 | 0.00003471 | 1.3177 |

Observation:

- as the system grows, the Wait-for Graph gets denser
- detection time and simulation time increase

### 7.3 DFS vs BFS

DFS is used as the primary method because it is simple and directly reveals a cycle path.
BFS/Kahn-style pruning is included as a comparison baseline.

## 8. Discussion

### 8.1 Trade-off

The main trade-off is:

- small `K` = faster detection, higher overhead
- large `K` = lower overhead, higher latency

### 8.2 Graph construction cost

Not only cycle detection matters. Building the Wait-for Graph also consumes time, especially when many processes and resources are active.

### 8.3 Practical interpretation

Continuous detection (`K=1`) is useful when immediate response is important.
Periodic detection (`K>1`) is more efficient for larger systems when small delay is acceptable.

## 9. Limitations

- Synthetic datasets may not perfectly match a real OS workload.
- The current implementation focuses on lock-based waiting.
- BFS is included for comparison, but DFS is the main reported method.

## 10. Conclusion

This project demonstrates that deadlock detection using a Wait-for Graph is effective, but the cost depends heavily on trigger frequency and system size. The experiments show that frequent detection gives the best latency, while periodic detection is much cheaper. For large systems, graph size and detection overhead grow noticeably, so detection strategy should be chosen based on the system's performance requirements.

