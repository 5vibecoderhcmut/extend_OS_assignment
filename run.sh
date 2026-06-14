#!/usr/bin/env bash
set -e
python3 run_experiments.py --dataset-dir datasets --output-dir results --algorithms dfs bfs --k 1 2 5 10 --repeat 5
