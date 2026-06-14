#!/usr/bin/env python3
"""Run Topic 3 experiments from existing CSV datasets.

This script scans a dataset folder recursively, runs each CSV file with selected
algorithms and detection intervals, repeats each configuration if requested, and writes:

1. results/experiment_results.csv  : averaged rows per dataset x algorithm x K
2. results/group_summary.csv       : averaged rows per group x algorithm x K

No dataset generation is performed here.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple

from deadlock_simulator import Event, read_events, simulate


DEFAULT_K_VALUES = [1, 2, 5, 10]
DEFAULT_ALGORITHMS = ["dfs", "bfs"]
EXCLUDED_CSV_NAMES = {
    "experiment_results.csv",
    "group_summary.csv",
    "experiment_runs.csv",
    "experiment_summary.csv",
    "expected_results.csv",
    "manifest_expected_results.csv",
}
GROUP_ORDER = {"small": 0, "medium": 1, "large": 2, "very_large": 3}

# Same structure for dataset-level result and group-level summary.
RESULT_FIELDNAMES = [
    "dataset_group",
    "dataset_file",
    "process_count",
    "resource_count",
    "event_count",
    "algorithm",
    "detection_interval",
    "runs",
    "deadlock_detected",
    "actual_deadlock_time",
    "detected_deadlock_time",
    "detection_latency",
    "detection_frequency",
    "detection_time_seconds",
    "detection_overhead_seconds",
    "max_wait_for_edges",
]

NUMERIC_AVERAGE_COLUMNS = [
    "process_count",
    "resource_count",
    "event_count",
    "actual_deadlock_time",
    "detected_deadlock_time",
    "detection_latency",
    "detection_frequency",
    "detection_time_seconds",
    "detection_overhead_seconds",
    "max_wait_for_edges",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run existing Topic 3 CSV datasets and collect averaged experiment output."
    )
    parser.add_argument("--dataset-dir", type=Path, default=Path("datasets"))
    parser.add_argument("--output-dir", type=Path, default=Path("results"))
    parser.add_argument("--algorithms", nargs="+", choices=DEFAULT_ALGORITHMS, default=DEFAULT_ALGORITHMS)
    parser.add_argument("--k", nargs="+", type=int, default=DEFAULT_K_VALUES)
    parser.add_argument("--repeat", type=int, default=5)
    parser.add_argument("--groups", nargs="+", default=None, help="Optional filter: small medium large very_large")
    parser.add_argument("--fail-fast", action="store_true")
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    if not args.dataset_dir.exists() or not args.dataset_dir.is_dir():
        raise SystemExit(f"Dataset directory not found: {args.dataset_dir}")
    if args.repeat < 1:
        raise SystemExit("--repeat must be >= 1")
    if any(k < 1 for k in args.k):
        raise SystemExit("All K values must be >= 1")


def dataset_group(dataset_file: Path, dataset_dir: Path) -> str:
    rel = dataset_file.relative_to(dataset_dir)
    return rel.parts[0] if len(rel.parts) > 1 else "root"


def group_sort_key(group: str) -> Tuple[int, str]:
    return (GROUP_ORDER.get(group, 99), group)


def find_dataset_files(dataset_dir: Path, groups: Optional[List[str]]) -> List[Path]:
    allowed_groups = set(groups) if groups else None
    files: List[Path] = []
    for path in dataset_dir.rglob("*.csv"):
        if path.name in EXCLUDED_CSV_NAMES:
            continue
        group = dataset_group(path, dataset_dir)
        if allowed_groups is not None and group not in allowed_groups:
            continue
        files.append(path)
    files.sort(key=lambda p: (group_sort_key(dataset_group(p, dataset_dir)), p.name))
    return files


def count_entities(events: Iterable[Event]) -> Tuple[int, int]:
    event_list = list(events)
    processes = {event.process_id for event in event_list}
    resources = {event.resource_id for event in event_list}
    return len(processes), len(resources)


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


def mean(values: List[float]) -> float:
    return sum(values) / len(values) if values else 0.0


def format_number(value: float, digits: int = 3) -> Any:
    # Keep integer-looking metrics clean in CSV, but preserve decimals when needed.
    if abs(value - round(value)) < 1e-12:
        return int(round(value))
    return f"{value:.{digits}f}"


def format_output_value(column: str, value: Any) -> Any:
    if value in (None, ""):
        return ""
    if isinstance(value, bool):
        return value
    numeric = to_float(value)
    if numeric is None:
        return value
    if column in {"detection_time_seconds", "detection_overhead_seconds"}:
        return f"{numeric:.9f}"
    return format_number(numeric, digits=3)


def format_output_row(row: Dict[str, Any]) -> Dict[str, Any]:
    return {column: format_output_value(column, row.get(column, "")) for column in RESULT_FIELDNAMES}


def first_non_empty(values: List[Any]) -> Any:
    for value in values:
        if value not in (None, ""):
            return value
    return ""


def average_numeric(results: List[Dict[str, Any]], key: str) -> float:
    values = [v for v in (to_float(result.get(key)) for result in results) if v is not None]
    return mean(values)


def run_configuration(
    dataset_file: Path,
    dataset_dir: Path,
    events: List[Event],
    algorithm: str,
    k: int,
    repeat: int,
) -> Dict[str, Any]:
    process_count, resource_count = count_entities(events)
    repeated_results: List[Dict[str, Any]] = []

    for _ in range(repeat):
        repeated_results.append(simulate(events, detection_interval=k, detector_name=algorithm))

    deadlock_detected = all(bool(result.get("deadlock_detected")) for result in repeated_results)

    return {
        "dataset_group": dataset_group(dataset_file, dataset_dir),
        "dataset_file": dataset_file.relative_to(dataset_dir).as_posix(),
        "process_count": process_count,
        "resource_count": resource_count,
        "event_count": len(events),
        "algorithm": algorithm,
        "detection_interval": k,
        "runs": repeat,
        "deadlock_detected": deadlock_detected,
        "actual_deadlock_time": first_non_empty([r.get("first_actual_deadlock_time") for r in repeated_results]),
        "detected_deadlock_time": first_non_empty([r.get("first_detected_deadlock_time") for r in repeated_results]),
        "detection_latency": first_non_empty([r.get("detection_latency") for r in repeated_results]),
        "detection_frequency": first_non_empty([r.get("detection_frequency") for r in repeated_results]),
        "detection_time_seconds": average_numeric(repeated_results, "avg_detection_time_seconds"),
        "detection_overhead_seconds": average_numeric(repeated_results, "detection_overhead_seconds"),
        "max_wait_for_edges": first_non_empty([r.get("max_wait_for_edges") for r in repeated_results]),
    }


def build_group_summary(rows: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    grouped: Dict[Tuple[str, str, int], List[Dict[str, Any]]] = {}
    for row in rows:
        key = (str(row["dataset_group"]), str(row["algorithm"]), int(row["detection_interval"]))
        grouped.setdefault(key, []).append(row)

    summary_rows: List[Dict[str, Any]] = []
    for (group, algorithm, k), group_rows in sorted(grouped.items(), key=lambda item: (group_sort_key(item[0][0]), item[0][1], item[0][2])):
        summary: Dict[str, Any] = {
            "dataset_group": group,
            "dataset_file": "GROUP_AVERAGE",
            "algorithm": algorithm,
            "detection_interval": k,
            # Here runs means total simulation runs used in this group-level average.
            "runs": sum(int(row["runs"]) for row in group_rows),
            "deadlock_detected": all(bool(row["deadlock_detected"]) for row in group_rows),
        }
        for column in NUMERIC_AVERAGE_COLUMNS:
            values = [v for v in (to_float(row.get(column)) for row in group_rows) if v is not None]
            summary[column] = mean(values) if values else ""
        summary_rows.append(summary)
    return summary_rows


def write_csv(path: Path, rows: List[Dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=RESULT_FIELDNAMES, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(format_output_row(row) for row in rows)


def write_config(output_dir: Path, args: argparse.Namespace, dataset_files: List[Path]) -> None:
    config = {
        "created_at": datetime.now().isoformat(timespec="seconds"),
        "dataset_dir": str(args.dataset_dir),
        "output_dir": str(args.output_dir),
        "algorithms": args.algorithms,
        "k_values": args.k,
        "repeat": args.repeat,
        "groups": args.groups,
        "dataset_count": len(dataset_files),
        "datasets": [str(path.relative_to(args.dataset_dir)) for path in dataset_files],
    }
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "experiment_config.json").write_text(
        json.dumps(config, indent=2, ensure_ascii=False), encoding="utf-8"
    )


def main() -> None:
    args = parse_args()
    validate_args(args)

    dataset_files = find_dataset_files(args.dataset_dir, args.groups)
    if not dataset_files:
        raise SystemExit(f"No dataset CSV files found in: {args.dataset_dir}")

    rows: List[Dict[str, Any]] = []
    error_count = 0

    for dataset_file in dataset_files:
        try:
            events = read_events(dataset_file)
            for algorithm in args.algorithms:
                for k in args.k:
                    rows.append(
                        run_configuration(
                            dataset_file=dataset_file,
                            dataset_dir=args.dataset_dir,
                            events=events,
                            algorithm=algorithm,
                            k=k,
                            repeat=args.repeat,
                        )
                    )
        except Exception as exc:
            error_count += 1
            if args.fail_fast:
                raise
            print(f"[ERROR] {dataset_file}: {exc}")

    summary_rows = build_group_summary(rows)

    output_path = args.output_dir / "experiment_results.csv"
    summary_path = args.output_dir / "group_summary.csv"
    write_csv(output_path, rows)
    write_csv(summary_path, summary_rows)
    write_config(args.output_dir, args, dataset_files)

    print(f"Datasets scanned : {len(dataset_files)}")
    print(f"Result rows      : {len(rows)}")
    print(f"Summary rows     : {len(summary_rows)}")
    print(f"Errors           : {error_count}")
    print(f"Output           : {output_path}")
    print(f"Group summary    : {summary_path}")
    print(f"Config           : {args.output_dir / 'experiment_config.json'}")


if __name__ == "__main__":
    main()
