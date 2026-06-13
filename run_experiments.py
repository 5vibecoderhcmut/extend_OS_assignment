#!/usr/bin/env python3
"""Run Topic 3 experiments and write a summary CSV."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path
from typing import Dict, List

from deadlock_simulator import read_events, simulate
from generate_dataset import generate_dataset


DEFAULT_K_VALUES = [1, 2, 5, 10, 20, 50]


def run_experiments(output: Path, algorithms: List[str], k_values: List[int]) -> List[Dict[str, object]]:
    datasets_dir = Path("datasets")
    datasets_dir.mkdir(exist_ok=True)

    small_dataset = datasets_dir / "small_deadlock.csv"
    large_dataset = datasets_dir / "large_deadlock.csv"

    # Keep the source tree small: regenerate the large dataset when experiments run.
    if not large_dataset.exists():
        generate_dataset(large_dataset, processes=20, resources=20, events=200, seed=42)

    datasets = [small_dataset, large_dataset]
    rows: List[Dict[str, object]] = []

    for dataset in datasets:
        events = read_events(dataset)
        for algorithm in algorithms:
            for k in k_values:
                result = simulate(events, detection_interval=k, detector_name=algorithm)
                result["dataset"] = dataset.name
                rows.append(result)

    output.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "dataset",
        "events",
        "detector",
        "detection_interval",
        "detection_frequency",
        "avg_detection_time_seconds",
        "total_detection_time_seconds",
        "detection_overhead_seconds",
        "first_actual_deadlock_time",
        "first_detected_deadlock_time",
        "detection_latency",
        "deadlock_detected",
        "cycle",
        "cycle_length",
        "avg_wait_for_nodes",
        "avg_wait_for_edges",
        "max_wait_for_nodes",
        "max_wait_for_edges",
    ]

    with output.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    return rows


def main() -> None:
    parser = argparse.ArgumentParser(description="Run Wait-for Graph deadlock detection experiments.")
    parser.add_argument("--output", type=Path, default=Path("results/experiment_summary.csv"))
    parser.add_argument("--algorithms", nargs="+", choices=["dfs", "bfs"], default=["dfs", "bfs"])
    parser.add_argument("--k", nargs="+", type=int, default=DEFAULT_K_VALUES)
    args = parser.parse_args()

    rows = run_experiments(args.output, args.algorithms, args.k)
    print(f"Wrote {len(rows)} experiment rows to {args.output}")


if __name__ == "__main__":
    main()
