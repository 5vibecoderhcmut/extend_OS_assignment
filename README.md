# Deadlock Detection using Wait-for Graph

This README only explains how to build, run experiments, and generate plots. Algorithm details and result analysis are covered in the report.

## Project Structure

```text
.
|-- datasets/                    # input CSV datasets
|   |-- small/
|   |-- medium/
|   |-- large/
|   `-- very_large/
|-- include/
|   `-- deadlock_core.h          # shared C header and data structures
|-- scripts/
|   `-- plot_results.py          # generate plots from CSV results
|-- src/
|   |-- deadlock_core.c          # core simulation and detection logic
|   |-- deadlock_simulator.c     # run and trace one dataset
|   `-- run_experiments.c        # run batch experiments
|-- Makefile
|-- run.sh
`-- README.md
```

## Requirements

You need `gcc`, `make`, and Python 3.

On Ubuntu/WSL:

```bash
sudo apt update
sudo apt install build-essential make python3-matplotlib
```

`python3-matplotlib` is only required for plot generation.

## Run All Experiments

From the project directory:

```bash
make all
```

Or:

```bash
./run.sh
```

CSV results will be created in:

```text
results/
|-- experiment_results.csv
|-- group_summary.csv
`-- experiment_config.json
```

## Generate Plots

Run this after the `results/` directory has been generated:

```bash
make plot
```

Plot images will be created in:

```text
plots/
```

## Run One Dataset

Build the simulator:

```bash
make simulator
```

Run one dataset:

```bash
./bin/deadlock_simulator datasets/small/small_random_5p.csv -k 5 --algorithm dfs --trace
```

You can choose the detection algorithm with:

```bash
--algorithm dfs
--algorithm bfs
```

## Useful Commands

```bash
make binaries      # build executables
make small         # run the small dataset group
make medium        # run the medium dataset group
make large         # run the large dataset group
make very_large    # run the very_large dataset group
make clean         # remove bin/, build/, results/, and plots/
```
