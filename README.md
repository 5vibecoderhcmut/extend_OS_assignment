# Deadlock Detection using Wait-for Graph

This project follows **Topic 3: Deadlock Detection** from the assignment specification.
It simulates how a Wait-for Graph is formed over logical time and detects deadlock by checking whether the graph contains a directed cycle.

## Input format

Only the required Topic 3 CSV format is used:

```csv
time,process_id,action,resource_id
0,P1,request,R1
1,P2,request,R2
```

No extra columns such as `thread_type`, `duration`, `timeout`, `lock_type`, or `priority` are used.
The only supported action is:

```text
request
```

## Simulation model

For each request event:

1. If the resource is free, it is granted to the requesting process.
2. If the resource is already held by another process, the requesting process waits.
3. If `Pi` waits for a resource held by `Pj`, the simulator adds edge `Pi -> Pj` to the Wait-for Graph.
4. Deadlock exists when the Wait-for Graph has a cycle.

There is no recovery, rollback, timeout, lock ordering, or victim selection in this version because those belong to other topics.

## Files

```text
deadlock_simulator.py   Core simulator and DFS/BFS cycle detection
generate_dataset.py     Generate a large Topic 3 dataset with the required CSV format
run_experiments.py      Run K-interval experiments and write results/experiment_summary.csv
datasets/               Small and large datasets using only the required columns
run.sh                  Simple shell script for demo
Makefile                Convenience commands
```

## Run a single dataset

Detect after every event:

```bash
python3 deadlock_simulator.py datasets/small_deadlock.csv -k 1 --algorithm dfs
```

Detect every 5 events:

```bash
python3 deadlock_simulator.py datasets/small_deadlock.csv -k 5 --algorithm dfs
```

Print the Wait-for Graph after each event:

```bash
python3 deadlock_simulator.py datasets/small_deadlock.csv -k 1 --algorithm dfs --trace
```

## Run experiments

```bash
python3 run_experiments.py
```

or:

```bash
make experiment
```

This writes:

```text
results/experiment_summary.csv
```

## Metrics

The experiment output includes the metrics required by Topic 3:

- `avg_detection_time_seconds`: average time for one detection call.
- `detection_frequency`: number of scheduled detection calls.
- `detection_latency`: first detected deadlock time minus first actual deadlock time.
- `detection_overhead_seconds`: `avg_detection_time_seconds * detection_frequency`.

Additional graph-size metrics are included to support scalability analysis:

- `avg_wait_for_nodes`
- `avg_wait_for_edges`
- `max_wait_for_nodes`
- `max_wait_for_edges`

## Dataset policy

The repository keeps only two ready-to-run datasets:

- `small_deadlock.csv`: 5-process sample dataset.
- `large_deadlock.csv`: generated dataset with at least 20 processes.

Both files use exactly:

```csv
time,process_id,action,resource_id
```
