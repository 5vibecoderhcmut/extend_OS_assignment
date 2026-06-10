#!/usr/bin/env python3
"""Generate synthetic lock datasets for scalability experiments."""

from __future__ import annotations

import argparse
import csv
import random
from pathlib import Path


def generate_dataset(
    output: Path,
    processes: int,
    resources: int,
    events: int,
    seed: int,
    force_deadlock: bool,
) -> None:
    random.seed(seed)
    process_ids = [f"P{i}" for i in range(1, processes + 1)]
    resource_ids = [f"R{i}" for i in range(1, resources + 1)]

    rows = []
    timestamp = 0

    if force_deadlock:
        cycle_len = min(processes, resources, max(2, min(5, processes, resources)))
        for i in range(cycle_len):
            rows.append([timestamp, process_ids[i], "WORKER", "LOCK", resource_ids[i], "Mutex", "null", "null"])
            timestamp += 10
        for i in range(cycle_len):
            rows.append([
                timestamp,
                process_ids[i],
                "WORKER",
                "LOCK",
                resource_ids[(i + 1) % cycle_len],
                "Mutex",
                "null",
                "null",
            ])
            timestamp += 10

    while len(rows) < events:
        duration = random.choice(["null", 50, 100, 500])
        rows.append([
            timestamp,
            random.choice(process_ids),
            random.choice(["UI", "WORKER"]),
            "LOCK",
            random.choice(resource_ids),
            random.choice(["Mutex", "Message_Queue"]),
            duration,
            random.choice(["null", 5000]),
        ])
        timestamp += random.randint(1, 20)

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow([
            "timestamp",
            "thread_id",
            "thread_type",
            "action",
            "target_resource",
            "resource_type",
            "duration_ms",
            "timeout_threshold",
        ])
        writer.writerows(rows[:events])


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a synthetic deadlock dataset.")
    parser.add_argument("output", type=Path)
    parser.add_argument("--processes", type=int, default=30)
    parser.add_argument("--resources", type=int, default=30)
    parser.add_argument("--events", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--no-deadlock", action="store_true")
    args = parser.parse_args()

    generate_dataset(
        args.output,
        args.processes,
        args.resources,
        args.events,
        args.seed,
        not args.no_deadlock,
    )


if __name__ == "__main__":
    main()
