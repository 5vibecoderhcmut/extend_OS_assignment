#!/usr/bin/env python3
"""Generate Topic 3 datasets using only the required CSV columns."""

from __future__ import annotations

import argparse
import csv
import random
from pathlib import Path
from typing import List


def generate_dataset(
    output: Path,
    processes: int,
    resources: int,
    events: int,
    seed: int = 42,
    force_deadlock: bool = True,
) -> None:
    if processes < 2 or resources < 2:
        raise ValueError("processes and resources must be >= 2")

    random.seed(seed)
    process_ids = [f"P{i}" for i in range(1, processes + 1)]
    resource_ids = [f"R{i}" for i in range(1, resources + 1)]

    rows: List[List[object]] = []
    logical_time = 0

    if force_deadlock:
        cycle_len = min(processes, resources)

        # Step 1: each process owns one resource.
        for i in range(cycle_len):
            rows.append([logical_time, process_ids[i], "request", resource_ids[i]])
            logical_time += 1

        # Step 2: create a circular wait P1->P2->...->Pn->P1.
        for i in range(cycle_len):
            rows.append([logical_time, process_ids[i], "request", resource_ids[(i + 1) % cycle_len]])
            logical_time += 1

    while len(rows) < events:
        rows.append([
            logical_time,
            random.choice(process_ids),
            "request",
            random.choice(resource_ids),
        ])
        logical_time += 1

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(["time", "process_id", "action", "resource_id"])
        writer.writerows(rows[:events])


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a Topic 3 Wait-for Graph dataset.")
    parser.add_argument("output", type=Path)
    parser.add_argument("--processes", type=int, default=20)
    parser.add_argument("--resources", type=int, default=20)
    parser.add_argument("--events", type=int, default=200)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--no-deadlock", action="store_true")
    args = parser.parse_args()

    generate_dataset(
        output=args.output,
        processes=args.processes,
        resources=args.resources,
        events=args.events,
        seed=args.seed,
        force_deadlock=not args.no_deadlock,
    )


if __name__ == "__main__":
    main()
