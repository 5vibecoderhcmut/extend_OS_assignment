#!/usr/bin/env python3
"""Run deadlock detection experiments and generate plots."""

from __future__ import annotations

import argparse
import csv
import os
from pathlib import Path
from typing import Dict, List

os.environ.setdefault("MPLCONFIGDIR", str(Path(".matplotlib_cache").resolve()))
os.environ.setdefault("XDG_CACHE_HOME", str(Path(".cache").resolve()))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from deadlock_simulator import read_events, simulate
from generate_dataset import generate_dataset


K_VALUES = [1, 2, 5, 10, 20, 50, 100, 200, 500]
SCALE_CONFIGS = [
    (5, 8, 200),
    (10, 15, 500),
    (20, 30, 1000),
    (50, 60, 3000),
    (100, 120, 6000),
    (150, 180, 9000),
    (200, 240, 12000),
]


def run_frequency_experiment(dataset: Path, output_dir: Path) -> List[Dict[str, object]]:
    events = read_events(dataset)
    rows: List[Dict[str, object]] = []

    for algorithm in ["dfs", "bfs"]:
        for interval in K_VALUES:
            result = simulate(events, interval, algorithm)
            result["experiment"] = "frequency"
            result["dataset"] = dataset.name
            rows.append(result)

    plot_frequency(rows, output_dir)
    return rows


def run_scale_experiment(output_dir: Path) -> List[Dict[str, object]]:
    rows: List[Dict[str, object]] = []

    generated_dir = output_dir / "generated"
    generated_dir.mkdir(parents=True, exist_ok=True)

    for processes, resources, event_count in SCALE_CONFIGS:
        dataset = generated_dir / f"scale_p{processes}_r{resources}_e{event_count}.csv"
        generate_dataset(
            dataset,
            processes=processes,
            resources=resources,
            events=event_count,
            seed=processes + resources + event_count,
            force_deadlock=True,
        )
        events = read_events(dataset)
        for algorithm in ["dfs", "bfs"]:
            result = simulate(events, detection_interval=10, detector_name=algorithm)
            result["experiment"] = "scale"
            result["dataset"] = dataset.name
            result["processes"] = processes
            result["resources"] = resources
            rows.append(result)

    plot_scale(rows, output_dir)
    return rows


def run_scale_by_k_experiment(output_dir: Path) -> List[Dict[str, object]]:
    rows: List[Dict[str, object]] = []
    generated_dir = output_dir / "generated"
    generated_dir.mkdir(parents=True, exist_ok=True)

    for processes, resources, event_count in SCALE_CONFIGS:
        dataset = generated_dir / f"scale_p{processes}_r{resources}_e{event_count}.csv"
        if not dataset.exists():
            generate_dataset(
                dataset,
                processes=processes,
                resources=resources,
                events=event_count,
                seed=processes + resources + event_count,
                force_deadlock=True,
            )
        events = read_events(dataset)
        for interval in K_VALUES:
            result = simulate(events, detection_interval=interval, detector_name="dfs")
            result["experiment"] = "scale_by_k"
            result["dataset"] = dataset.name
            result["processes"] = processes
            result["resources"] = resources
            rows.append(result)

    plot_scale_by_k(rows, output_dir)
    return rows


def write_summary(rows: List[Dict[str, object]], output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    summary_path = output_dir / "experiment_summary.csv"
    keys = sorted({key for row in rows for key in row})
    with summary_path.open("w", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=keys)
        writer.writeheader()
        writer.writerows(rows)


def plot_frequency(rows: List[Dict[str, object]], output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    dfs_rows = [row for row in rows if row["detector"] == "dfs"]
    x = [int(row["detection_interval"]) for row in dfs_rows]

    fig, axes = plt.subplots(1, 3, figsize=(15, 4))
    fig.suptitle("Detection frequency trade-off (DFS)")

    axes[0].plot(x, [float(row["detection_overhead"]) for row in dfs_rows], marker="o")
    axes[0].set_title("Overhead vs K")
    axes[0].set_xlabel("Detection interval K")
    axes[0].set_ylabel("Overhead (seconds)")
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(x, [int(row["detection_frequency"]) for row in dfs_rows], marker="o", color="#2a9d8f")
    axes[1].set_title("Frequency vs K")
    axes[1].set_xlabel("Detection interval K")
    axes[1].set_ylabel("Detection calls")
    axes[1].grid(True, alpha=0.3)

    latencies = [0 if row["detection_latency"] is None else int(row["detection_latency"]) for row in dfs_rows]
    axes[2].plot(x, latencies, marker="o", color="#e76f51")
    axes[2].set_title("Latency vs K")
    axes[2].set_xlabel("Detection interval K")
    axes[2].set_ylabel("Latency timestamp units")
    axes[2].grid(True, alpha=0.3)

    fig.tight_layout()
    fig.savefig(output_dir / "frequency_tradeoff.png", dpi=160)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(7, 4))
    latencies = [0 if row["detection_latency"] is None else int(row["detection_latency"]) for row in dfs_rows]
    overheads = [float(row["detection_overhead"]) for row in dfs_rows]
    ax.scatter(overheads, latencies, s=60)
    for row, overhead, latency in zip(dfs_rows, overheads, latencies):
        ax.annotate(f"K={row['detection_interval']}", (overhead, latency), fontsize=8)
    ax.set_title("Overhead vs latency trade-off (DFS)")
    ax.set_xlabel("Detection overhead (seconds)")
    ax.set_ylabel("Detection latency timestamp units")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_dir / "overhead_latency_tradeoff.png", dpi=160)
    plt.close(fig)

    dfs = [row for row in rows if row["detector"] == "dfs"]
    bfs = [row for row in rows if row["detector"] == "bfs"]

    fig, ax = plt.subplots(figsize=(7, 4))
    ax.plot(
        [int(row["detection_interval"]) for row in dfs],
        [float(row["avg_detection_time_seconds"]) for row in dfs],
        marker="o",
        label="DFS",
    )
    ax.plot(
        [int(row["detection_interval"]) for row in bfs],
        [float(row["avg_detection_time_seconds"]) for row in bfs],
        marker="s",
        label="BFS",
    )
    ax.set_title("DFS vs BFS average detection time")
    ax.set_xlabel("Detection interval K")
    ax.set_ylabel("Average detection time (seconds)")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output_dir / "dfs_vs_bfs.png", dpi=160)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(7, 4))
    ax.plot(
        x,
        [float(row["avg_graph_build_time_seconds"]) for row in dfs_rows],
        marker="o",
        color="#6a4c93",
    )
    ax.set_title("Wait-for Graph build time vs K")
    ax.set_xlabel("Detection interval K")
    ax.set_ylabel("Average graph build time per event (seconds)")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_dir / "graph_build_time.png", dpi=160)
    plt.close(fig)


def plot_scale(rows: List[Dict[str, object]], output_dir: Path) -> None:
    dfs_rows = [row for row in rows if row["detector"] == "dfs"]
    bfs_rows = [row for row in rows if row["detector"] == "bfs"]

    fig, axes = plt.subplots(1, 3, figsize=(16, 4))
    fig.suptitle("Scalability with K=10")

    axes[0].plot(
        [int(row["processes"]) for row in dfs_rows],
        [float(row["avg_detection_time_seconds"]) for row in dfs_rows],
        marker="o",
        label="DFS",
    )
    axes[0].plot(
        [int(row["processes"]) for row in bfs_rows],
        [float(row["avg_detection_time_seconds"]) for row in bfs_rows],
        marker="s",
        label="BFS",
    )
    axes[0].set_title("Detection time vs processes")
    axes[0].set_xlabel("Processes")
    axes[0].set_ylabel("Average detection time (seconds)")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend()

    axes[1].plot(
        [int(row["max_wait_for_edges"]) for row in dfs_rows],
        [float(row["avg_detection_time_seconds"]) for row in dfs_rows],
        marker="o",
        label="DFS",
    )
    axes[1].plot(
        [int(row["max_wait_for_edges"]) for row in bfs_rows],
        [float(row["avg_detection_time_seconds"]) for row in bfs_rows],
        marker="s",
        label="BFS",
    )
    axes[1].set_title("Detection time vs graph edges")
    axes[1].set_xlabel("Max Wait-for Graph edges")
    axes[1].set_ylabel("Average detection time (seconds)")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend()

    axes[2].plot(
        [int(row["processes"]) for row in dfs_rows],
        [float(row["total_simulation_time_seconds"]) for row in dfs_rows],
        marker="o",
        label="DFS",
    )
    axes[2].plot(
        [int(row["processes"]) for row in bfs_rows],
        [float(row["total_simulation_time_seconds"]) for row in bfs_rows],
        marker="s",
        label="BFS",
    )
    axes[2].set_title("Total simulation time")
    axes[2].set_xlabel("Processes")
    axes[2].set_ylabel("Seconds")
    axes[2].grid(True, alpha=0.3)
    axes[2].legend()

    fig.tight_layout()
    fig.savefig(output_dir / "scalability.png", dpi=160)
    plt.close(fig)


def plot_scale_by_k(rows: List[Dict[str, object]], output_dir: Path) -> None:
    processes = sorted({int(row["processes"]) for row in rows})
    intervals = sorted({int(row["detection_interval"]) for row in rows})

    latency_grid = []
    overhead_grid = []
    for process_count in processes:
        process_rows = [
            row for row in rows if int(row["processes"]) == process_count
        ]
        latency_grid.append([
            0
            if row["detection_latency"] is None
            else int(row["detection_latency"])
            for interval in intervals
            for row in process_rows
            if int(row["detection_interval"]) == interval
        ])
        overhead_grid.append([
            float(row["detection_overhead"])
            for interval in intervals
            for row in process_rows
            if int(row["detection_interval"]) == interval
        ])

    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    fig.suptitle("Scale x K matrix (DFS)")

    latency_image = axes[0].imshow(latency_grid, aspect="auto", cmap="YlOrRd")
    axes[0].set_title("Detection latency")
    axes[0].set_xticks(range(len(intervals)), intervals)
    axes[0].set_yticks(range(len(processes)), processes)
    axes[0].set_xlabel("Detection interval K")
    axes[0].set_ylabel("Processes")
    fig.colorbar(latency_image, ax=axes[0], fraction=0.046, pad=0.04)

    overhead_image = axes[1].imshow(overhead_grid, aspect="auto", cmap="Blues")
    axes[1].set_title("Detection overhead")
    axes[1].set_xticks(range(len(intervals)), intervals)
    axes[1].set_yticks(range(len(processes)), processes)
    axes[1].set_xlabel("Detection interval K")
    axes[1].set_ylabel("Processes")
    fig.colorbar(overhead_image, ax=axes[1], fraction=0.046, pad=0.04)

    fig.tight_layout()
    fig.savefig(output_dir / "scale_by_k_matrix.png", dpi=160)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Run experiments and generate plots.")
    parser.add_argument(
        "--dataset",
        type=Path,
        default=Path("datasets/generated_large.csv"),
        help="Dataset used for frequency/latency trade-off plots.",
    )
    parser.add_argument("--output-dir", type=Path, default=Path("results"))
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    if not args.dataset.exists():
        generate_dataset(
            args.dataset,
            processes=50,
            resources=60,
            events=5000,
            seed=42,
            force_deadlock=True,
        )

    rows = []
    rows.extend(run_frequency_experiment(args.dataset, args.output_dir))
    rows.extend(run_scale_experiment(args.output_dir))
    rows.extend(run_scale_by_k_experiment(args.output_dir))
    write_summary(rows, args.output_dir)

    print(f"Wrote plots and summary to {args.output_dir}")
    print(f"- {args.output_dir / 'frequency_tradeoff.png'}")
    print(f"- {args.output_dir / 'overhead_latency_tradeoff.png'}")
    print(f"- {args.output_dir / 'dfs_vs_bfs.png'}")
    print(f"- {args.output_dir / 'graph_build_time.png'}")
    print(f"- {args.output_dir / 'scalability.png'}")
    print(f"- {args.output_dir / 'scale_by_k_matrix.png'}")
    print(f"- {args.output_dir / 'experiment_summary.csv'}")


if __name__ == "__main__":
    main()
