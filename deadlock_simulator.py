#!/usr/bin/env python3
"""Deadlock detection simulator using a Wait-for Graph.

This version follows the Topic 3 specification only.

Input CSV format:
    time,process_id,action,resource_id

Supported action:
    request

Model:
- If a requested resource is free, it is granted to the process.
- If a requested resource is already held by another process, the requester waits.
- The Wait-for Graph contains edge Pi -> Pj if Pi waits for a resource held by Pj.
- Deadlock exists when the Wait-for Graph contains a directed cycle.
"""

from __future__ import annotations

import argparse
import csv
import time
from collections import defaultdict, deque
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, DefaultDict, Dict, Iterable, List, Optional, Set, Tuple


REQUIRED_COLUMNS = ["time", "process_id", "action", "resource_id"]
SUPPORTED_ACTION = "REQUEST"


@dataclass(frozen=True)
class Event:
    time: int
    process_id: str
    action: str
    resource_id: str


@dataclass(frozen=True)
class DetectionResult:
    has_deadlock: bool
    cycle: List[str]


Graph = Dict[str, Set[str]]
Detector = Callable[[Graph], DetectionResult]


def read_events(csv_path: Path) -> List[Event]:
    """Read the exact Topic 3 CSV format."""
    events: List[Event] = []

    with csv_path.open(newline="", encoding="utf-8") as file:
        reader = csv.DictReader(file)
        if reader.fieldnames is None:
            raise ValueError(f"Empty CSV file: {csv_path}")

        missing = [column for column in REQUIRED_COLUMNS if column not in reader.fieldnames]
        if missing:
            raise ValueError(
                f"Invalid CSV format in {csv_path}. Missing columns: {missing}. "
                f"Expected header: {','.join(REQUIRED_COLUMNS)}"
            )

        for line_number, row in enumerate(reader, start=2):
            try:
                logical_time = int(str(row["time"]).strip())
            except ValueError as exc:
                raise ValueError(f"Invalid time at line {line_number}: {row}") from exc

            process_id = str(row["process_id"]).strip()
            action = str(row["action"]).strip().upper()
            resource_id = str(row["resource_id"]).strip()

            if not process_id or not resource_id:
                raise ValueError(f"Missing process_id or resource_id at line {line_number}: {row}")

            if action != SUPPORTED_ACTION:
                raise ValueError(
                    f"Unsupported action at line {line_number}: {row['action']!r}. "
                    "Topic 3 uses only action=request."
                )

            events.append(Event(logical_time, process_id, action, resource_id))

    return sorted(events, key=lambda event: event.time)


def detect_cycle_dfs(graph: Graph) -> DetectionResult:
    """Detect one directed cycle using DFS with color marking."""
    color: Dict[str, int] = {}  # 0 = unvisited, 1 = visiting, 2 = done
    stack: List[str] = []
    stack_index: Dict[str, int] = {}

    def visit(node: str) -> Optional[List[str]]:
        color[node] = 1
        stack_index[node] = len(stack)
        stack.append(node)

        for neighbor in sorted(graph.get(node, set())):
            state = color.get(neighbor, 0)
            if state == 0:
                cycle = visit(neighbor)
                if cycle is not None:
                    return cycle
            elif state == 1:
                return stack[stack_index[neighbor] :] + [neighbor]

        stack.pop()
        stack_index.pop(node, None)
        color[node] = 2
        return None

    nodes = graph_nodes(graph)
    for node in sorted(nodes):
        if color.get(node, 0) == 0:
            cycle = visit(node)
            if cycle is not None:
                return DetectionResult(True, cycle)

    return DetectionResult(False, [])


def detect_cycle_bfs(graph: Graph) -> DetectionResult:
    """Optional cycle detection using Kahn's topological pruning."""
    nodes = graph_nodes(graph)
    indegree: DefaultDict[str, int] = defaultdict(int)

    for node in nodes:
        indegree[node] = indegree[node]

    for source, neighbors in graph.items():
        for target in neighbors:
            indegree[target] += 1

    queue = deque(sorted(node for node in nodes if indegree[node] == 0))
    removed = 0

    while queue:
        node = queue.popleft()
        removed += 1
        for neighbor in sorted(graph.get(node, set())):
            indegree[neighbor] -= 1
            if indegree[neighbor] == 0:
                queue.append(neighbor)

    if removed == len(nodes):
        return DetectionResult(False, [])

    # Kahn's algorithm tells us a cycle exists. For clearer output,
    # extract one concrete cycle from the remaining cyclic subgraph.
    remaining_nodes = {node for node in nodes if indegree[node] > 0}
    cyclic_subgraph = {
        node: {neighbor for neighbor in graph.get(node, set()) if neighbor in remaining_nodes}
        for node in remaining_nodes
    }
    return detect_cycle_dfs(cyclic_subgraph)


def graph_nodes(graph: Graph) -> Set[str]:
    nodes: Set[str] = set(graph)
    for neighbors in graph.values():
        nodes.update(neighbors)
    return nodes


def graph_size(graph: Graph) -> Tuple[int, int]:
    return len(graph_nodes(graph)), sum(len(neighbors) for neighbors in graph.values())


class WaitForGraphSimulator:
    """Maintain resource ownership and derive the Wait-for Graph over time."""

    def __init__(self) -> None:
        self.resource_owner: Dict[str, str] = {}
        self.process_holds: DefaultDict[str, Set[str]] = defaultdict(set)
        self.waiting_for_resources: DefaultDict[str, Set[str]] = defaultdict(set)

    def apply_event(self, event: Event) -> None:
        self.request(event.process_id, event.resource_id)

    def request(self, process_id: str, resource_id: str) -> None:
        owner = self.resource_owner.get(resource_id)

        if owner is None:
            self.resource_owner[resource_id] = process_id
            self.process_holds[process_id].add(resource_id)
            return

        if owner == process_id:
            return

        self.waiting_for_resources[process_id].add(resource_id)

    def wait_for_graph(self) -> Graph:
        graph: DefaultDict[str, Set[str]] = defaultdict(set)

        for waiting_process, resources in self.waiting_for_resources.items():
            for resource_id in resources:
                owner = self.resource_owner.get(resource_id)
                if owner is not None and owner != waiting_process:
                    graph[waiting_process].add(owner)

        return {node: set(neighbors) for node, neighbors in graph.items() if neighbors}

    def ownership_snapshot(self) -> Dict[str, str]:
        return dict(sorted(self.resource_owner.items()))


def choose_detector(name: str) -> Detector:
    if name == "dfs":
        return detect_cycle_dfs
    if name == "bfs":
        return detect_cycle_bfs
    raise ValueError(f"Unknown detector: {name}")


def should_run_detection(event_index: int, total_events: int, detection_interval: int) -> bool:
    return event_index % detection_interval == 0 or event_index == total_events


def simulate(
    events: Iterable[Event],
    detection_interval: int = 1,
    detector_name: str = "dfs",
    trace: bool = False,
) -> Dict[str, object]:
    event_list = list(events)
    simulator = WaitForGraphSimulator()
    detector = choose_detector(detector_name)

    detection_frequency = 0
    total_detection_time_seconds = 0.0
    first_actual_deadlock_time: Optional[int] = None
    first_detected_deadlock_time: Optional[int] = None
    last_cycle: List[str] = []

    max_wait_for_nodes = 0
    max_wait_for_edges = 0
    total_wait_for_nodes = 0
    total_wait_for_edges = 0

    for event_index, event in enumerate(event_list, start=1):
        simulator.apply_event(event)
        graph = simulator.wait_for_graph()
        node_count, edge_count = graph_size(graph)

        max_wait_for_nodes = max(max_wait_for_nodes, node_count)
        max_wait_for_edges = max(max_wait_for_edges, edge_count)
        total_wait_for_nodes += node_count
        total_wait_for_edges += edge_count

        # This offline check identifies when the deadlock first becomes true.
        # It is not counted as a scheduled detection call.
        actual = detect_cycle_dfs(graph)
        if actual.has_deadlock and first_actual_deadlock_time is None:
            first_actual_deadlock_time = event.time

        if should_run_detection(event_index, len(event_list), detection_interval):
            start = time.perf_counter()
            detected = detector(graph)
            elapsed = time.perf_counter() - start

            detection_frequency += 1
            total_detection_time_seconds += elapsed

            if detected.has_deadlock:
                last_cycle = detected.cycle
                if first_detected_deadlock_time is None:
                    first_detected_deadlock_time = event.time

        if trace:
            formatted_graph = format_graph(graph)
            print(f"time={event.time:>4} event={event.process_id} request {event.resource_id}")
            print(f"  owner: {simulator.ownership_snapshot()}")
            print(f"  WFG  : {formatted_graph if formatted_graph else '{}'}")

    event_count = len(event_list)
    avg_detection_time_seconds = (
        total_detection_time_seconds / detection_frequency if detection_frequency else 0.0
    )
    detection_overhead_seconds = avg_detection_time_seconds * detection_frequency

    detection_latency: Optional[int]
    if first_actual_deadlock_time is not None and first_detected_deadlock_time is not None:
        detection_latency = first_detected_deadlock_time - first_actual_deadlock_time
    else:
        detection_latency = None

    avg_wait_for_nodes = total_wait_for_nodes / event_count if event_count else 0.0
    avg_wait_for_edges = total_wait_for_edges / event_count if event_count else 0.0

    return {
        "events": event_count,
        "detector": detector_name,
        "detection_interval": detection_interval,
        "detection_frequency": detection_frequency,
        "avg_detection_time_seconds": avg_detection_time_seconds,
        "total_detection_time_seconds": total_detection_time_seconds,
        "detection_overhead_seconds": detection_overhead_seconds,
        "first_actual_deadlock_time": first_actual_deadlock_time,
        "first_detected_deadlock_time": first_detected_deadlock_time,
        "detection_latency": detection_latency,
        "deadlock_detected": first_detected_deadlock_time is not None,
        "cycle": " -> ".join(last_cycle),
        "cycle_length": max(0, len(last_cycle) - 1),
        "avg_wait_for_nodes": avg_wait_for_nodes,
        "avg_wait_for_edges": avg_wait_for_edges,
        "max_wait_for_nodes": max_wait_for_nodes,
        "max_wait_for_edges": max_wait_for_edges,
    }


def format_graph(graph: Graph) -> str:
    edges: List[str] = []
    for source in sorted(graph):
        for target in sorted(graph[source]):
            edges.append(f"{source}->{target}")
    return ", ".join(edges)


def print_result(result: Dict[str, object]) -> None:
    for key, value in result.items():
        if isinstance(value, float):
            print(f"{key}: {value:.9f}")
        else:
            print(f"{key}: {value}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Detect deadlock using a Wait-for Graph.")
    parser.add_argument("dataset", type=Path, help="CSV file with time,process_id,action,resource_id")
    parser.add_argument("-k", "--detection-interval", type=int, default=1, help="Run detection every K events")
    parser.add_argument("--algorithm", choices=["dfs", "bfs"], default="dfs", help="Cycle detection algorithm")
    parser.add_argument("--trace", action="store_true", help="Print resource ownership and WFG after each event")
    args = parser.parse_args()

    if args.detection_interval < 1:
        raise SystemExit("detection interval must be >= 1")

    events = read_events(args.dataset)
    result = simulate(events, args.detection_interval, args.algorithm, args.trace)
    print_result(result)


if __name__ == "__main__":
    main()
