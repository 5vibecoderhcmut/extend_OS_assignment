#!/usr/bin/env bash
set -euo pipefail

K="${1:-1}"
python3 deadlock_simulator.py datasets/small_deadlock.csv -k "$K" --algorithm dfs
