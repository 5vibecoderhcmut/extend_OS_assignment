# Topic 3: Deadlock Detection using Wait-for Graph
## Dataset groups

The dataset folder now contains four groups. Each group has five CSV files: one original controlled-random case plus four additional pseudo-random cases.

```text
datasets/
├── small/       # <= 5 processes, 5 files
├── medium/      # 6 to 19 processes, 5 files
├── large/       # 20 to 35 processes, 5 files
└── very_large/  # > 35 processes, 5 files
```

All CSV files use the Topic 3 format:

```csv
time,process_id,action,resource_id
0,P1,request,R1
```

The datasets are pseudo-random but controlled:

- each process first acquires a unique resource;
- extra wait edges are added in an acyclic way;
- one closing wait edge creates the first deadlock;
- additional neutral events are added after the deadlock so K = 2, 5, and 10 can produce non-zero latency.

The cycles are not simple full-ring cycles over all processes.

##Setup Environment
### Create a virtual environment

Using Python venv:
```bash
python -m venv .venv
```

Activate the virtual environment:

Linux/macOS
```bash
source .venv/bin/activate
```

Windows (PowerShell)
```bash
.venv\Scripts\Activate.ps1
```

### Install dependencies

Install required Python packages:
```bash
pip install -r requirements.txt
```
## Run experiments

```bash
make all
```

or:

```bash
./run.sh
```

To run only one group:

```bash
make small
make medium
make large
make very_large
```

## Output files

The script creates:

```text
results/
├── experiment_results.csv
├── group_summary.csv
└── experiment_config.json
```

### experiment_results.csv

One row per:

```text
1 dataset × 1 algorithm × 1 K
```

The output is already averaged over repeated runs. If `--repeat 5`, it does not print five raw duplicate rows.

### group_summary.csv

One row per:

```text
1 dataset group × 1 algorithm × 1 K
```

This file has the same column structure as `experiment_results.csv`, but the numeric values are averaged across all five datasets in the group.

Columns:

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

In `group_summary.csv`, `dataset_file` is set to `GROUP_AVERAGE`, and `runs` means the total number of simulation runs used for that group average.

## Plotting

Plotting is still optional and is not run by default.

```bash
make plot
```


## Group-based plots

After running experiments, create report-ready plots with:

```bash
make plot
```

The plotting script reads `results/group_summary.csv` for group-level charts and `results/experiment_results.csv` for the WFG-edge scatter plot. The generated figures are:

```text
plots/latency_vs_k_by_group.png
plots/overhead_vs_k_by_group.png
plots/frequency_vs_k_by_group.png
plots/detection_time_by_group_dfs_bfs.png
plots/overhead_by_group_dfs_bfs.png
plots/detection_time_vs_edges.png
```
