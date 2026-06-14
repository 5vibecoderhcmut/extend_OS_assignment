from __future__ import annotations

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


Row = Dict[str, str]
GROUP_ORDER = ["small", "medium", "large", "very_large"]
ALGORITHM_ORDER = ["dfs", "bfs"]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot group-based charts from experiment output CSV files.")
    parser.add_argument(
        "--results-dir",
        type=Path,
        default=Path("results"),
        help="Directory containing group_summary.csv and experiment_results.csv.",
    )
    # Backward-compatible option. If Makefile still passes --input-csv results/experiment_results.csv,
    # the script will infer results_dir from its parent directory.
    parser.add_argument(
        "--input-csv",
        type=Path,
        default=None,
        help="Backward-compatible input path. If provided, its parent is used as results-dir.",
    )
    parser.add_argument("--output-dir", type=Path, default=Path("plots"))
    parser.add_argument("--format", default="png", choices=["png", "pdf", "svg"])
    parser.add_argument(
        "--primary-algorithm",
        default="dfs",
        choices=ALGORITHM_ORDER,
        help="Algorithm used for latency/frequency/overhead-vs-K group line charts.",
    )
    parser.add_argument(
        "--scale-k",
        type=int,
        default=1,
        help="Detection interval K used for group scale comparison charts.",
    )
    return parser.parse_args()


def read_rows(path: Path, required: bool = True) -> List[Row]:
    if not path.exists():
        if required:
            raise SystemExit(f"Input CSV not found: {path}. Run `make all` first.")
        return []
    with path.open(newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file))


def to_float(value: Any) -> Optional[float]:
    if value in (None, ""):
        return None
    try:
        numeric = float(value)
    except (TypeError, ValueError):
        return None
    if math.isnan(numeric):
        return None
    return numeric


def sorted_groups(rows: Iterable[Row]) -> List[str]:
    existing = {str(row.get("dataset_group", "")) for row in rows if row.get("dataset_group")}
    ordered = [group for group in GROUP_ORDER if group in existing]
    extra = sorted(group for group in existing if group not in GROUP_ORDER)
    return ordered + extra


def sorted_algorithms(rows: Iterable[Row]) -> List[str]:
    existing = {str(row.get("algorithm", "")) for row in rows if row.get("algorithm")}
    ordered = [algo for algo in ALGORITHM_ORDER if algo in existing]
    extra = sorted(algo for algo in existing if algo not in ALGORITHM_ORDER)
    return ordered + extra


def save_plot(output_dir: Path, name: str, fmt: str) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / f"{name}.{fmt}"
    plt.tight_layout()
    plt.savefig(path, dpi=200)
    plt.close()
    return path


def maybe_sci_yaxis() -> None:
    plt.ticklabel_format(axis="y", style="sci", scilimits=(-3, 3))


def plot_metric_vs_k_by_group(
    rows: List[Row],
    output_dir: Path,
    fmt: str,
    *,
    metric: str,
    title: str,
    ylabel: str,
    filename: str,
    algorithm: str,
    scientific_y: bool = False,
) -> Optional[Path]:
    filtered = [
        row for row in rows
        if row.get("algorithm") == algorithm and to_float(row.get(metric)) is not None
    ]
    if not filtered:
        return None

    by_group: Dict[str, List[Tuple[float, float]]] = defaultdict(list)
    for row in filtered:
        k = to_float(row.get("detection_interval"))
        value = to_float(row.get(metric))
        group = row.get("dataset_group", "")
        if k is not None and value is not None and group:
            by_group[group].append((k, value))

    if not by_group:
        return None

    plt.figure(figsize=(8.5, 5.2))
    for group in sorted_groups(filtered):
        points = sorted(by_group.get(group, []))
        if not points:
            continue
        xs, ys = zip(*points)
        plt.plot(xs, ys, marker="o", label=group)

    plt.title(f"{title} ({algorithm.upper()})")
    plt.xlabel("Detection interval K")
    plt.ylabel(ylabel)
    plt.grid(True, alpha=0.3)
    plt.legend(title="Dataset group")
    if scientific_y:
        maybe_sci_yaxis()
    return save_plot(output_dir, filename, fmt)


def plot_grouped_bar_by_algorithm(
    rows: List[Row],
    output_dir: Path,
    fmt: str,
    *,
    metric: str,
    title: str,
    ylabel: str,
    filename: str,
    k_value: int,
    scientific_y: bool = False,
) -> Optional[Path]:
    filtered = [
        row for row in rows
        if str(row.get("detection_interval")) == str(k_value)
        and to_float(row.get(metric)) is not None
    ]
    if not filtered:
        return None

    groups = sorted_groups(filtered)
    algorithms = sorted_algorithms(filtered)
    if not groups or not algorithms:
        return None

    values: Dict[Tuple[str, str], float] = {}
    for row in filtered:
        group = str(row.get("dataset_group", ""))
        algo = str(row.get("algorithm", ""))
        value = to_float(row.get(metric))
        if group and algo and value is not None:
            values[(group, algo)] = value

    x_positions = list(range(len(groups)))
    bar_width = 0.75 / max(len(algorithms), 1)

    plt.figure(figsize=(8.5, 5.2))
    for j, algo in enumerate(algorithms):
        offsets = [x + (j - (len(algorithms) - 1) / 2) * bar_width for x in x_positions]
        heights = [values.get((group, algo), 0.0) for group in groups]
        plt.bar(offsets, heights, width=bar_width, label=algo.upper())

    plt.title(f"{title} (K={k_value})")
    plt.xlabel("Dataset group")
    plt.ylabel(ylabel)
    plt.xticks(x_positions, groups)
    plt.grid(True, axis="y", alpha=0.3)
    plt.legend(title="Algorithm")
    if scientific_y:
        maybe_sci_yaxis()
    return save_plot(output_dir, filename, fmt)


def plot_detection_time_vs_edges(
    rows: List[Row],
    output_dir: Path,
    fmt: str,
    *,
    k_value: int,
) -> Optional[Path]:
    filtered = [
        row for row in rows
        if str(row.get("detection_interval")) == str(k_value)
        and to_float(row.get("max_wait_for_edges")) is not None
        and to_float(row.get("detection_time_seconds")) is not None
    ]
    if not filtered:
        return None

    # One series per group and algorithm. This keeps the chart useful for explaining
    # why larger process count does not always mean larger detection time.
    by_series: Dict[Tuple[str, str], List[Tuple[float, float]]] = defaultdict(list)
    for row in filtered:
        group = str(row.get("dataset_group", ""))
        algo = str(row.get("algorithm", ""))
        edges = to_float(row.get("max_wait_for_edges"))
        value = to_float(row.get("detection_time_seconds"))
        if group and algo and edges is not None and value is not None:
            by_series[(group, algo)].append((edges, value))

    if not by_series:
        return None

    plt.figure(figsize=(8.5, 5.2))
    groups = sorted_groups(filtered)
    algorithms = sorted_algorithms(filtered)
    markers = {"dfs": "o", "bfs": "s"}

    for group in groups:
        for algo in algorithms:
            points = sorted(by_series.get((group, algo), []))
            if not points:
                continue
            xs, ys = zip(*points)
            plt.scatter(xs, ys, marker=markers.get(algo, "o"), label=f"{group}-{algo.upper()}")

    plt.title(f"Detection time vs Wait-for Graph edges (K={k_value})")
    plt.xlabel("Maximum number of WFG edges")
    plt.ylabel("Average detection time per call (seconds)")
    plt.grid(True, alpha=0.3)
    plt.legend(title="Group / algorithm", fontsize=8)
    maybe_sci_yaxis()
    return save_plot(output_dir, "detection_time_vs_edges", fmt)


def main() -> None:
    args = parse_args()
    results_dir = args.input_csv.parent if args.input_csv is not None else args.results_dir

    group_rows = read_rows(results_dir / "group_summary.csv", required=True)
    experiment_rows = read_rows(results_dir / "experiment_results.csv", required=False)

    created: List[Path] = []

    chart_specs = [
        dict(
            metric="detection_latency",
            title="Detection latency vs K by group",
            ylabel="Average latency (time units)",
            filename="latency_vs_k_by_group",
            algorithm=args.primary_algorithm,
            scientific_y=False,
        ),
        dict(
            metric="detection_overhead_seconds",
            title="Detection overhead vs K by group",
            ylabel="Average total detection overhead (seconds)",
            filename="overhead_vs_k_by_group",
            algorithm=args.primary_algorithm,
            scientific_y=True,
        ),
        dict(
            metric="detection_frequency",
            title="Detection frequency vs K by group",
            ylabel="Average number of detection calls",
            filename="frequency_vs_k_by_group",
            algorithm=args.primary_algorithm,
            scientific_y=False,
        ),
    ]

    for spec in chart_specs:
        path = plot_metric_vs_k_by_group(group_rows, args.output_dir, args.format, **spec)
        if path is not None:
            created.append(path)

    bar_specs = [
        dict(
            metric="detection_time_seconds",
            title="Average detection time by group",
            ylabel="Average detection time per call (seconds)",
            filename="detection_time_by_group_dfs_bfs",
            k_value=args.scale_k,
            scientific_y=True,
        ),
        dict(
            metric="detection_overhead_seconds",
            title="Detection overhead by group",
            ylabel="Average total detection overhead (seconds)",
            filename="overhead_by_group_dfs_bfs",
            k_value=args.scale_k,
            scientific_y=True,
        ),
    ]

    for spec in bar_specs:
        path = plot_grouped_bar_by_algorithm(group_rows, args.output_dir, args.format, **spec)
        if path is not None:
            created.append(path)

    if experiment_rows:
        path = plot_detection_time_vs_edges(experiment_rows, args.output_dir, args.format, k_value=args.scale_k)
        if path is not None:
            created.append(path)

    if not created:
        raise SystemExit("No plots were created. Check that make all produced result CSV files.")

    print(f"Charts created: {len(created)}")
    for path in created:
        print(f"- {path}")


if __name__ == "__main__":
    main()
