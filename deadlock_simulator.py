#!/usr/bin/env python3
"""Deadlock detection simulator using a Wait-for Graph.

The simulator supports this repository's CSV format:

timestamp,thread_id,thread_type,action,target_resource,resource_type,duration_ms,timeout_threshold

It also accepts the simpler spec format:

time,process_id,action,resource_id
"""

from __future__ import annotations

import argparse
import csv
import time
from collections import defaultdict, deque
from dataclasses import dataclass
from pathlib import Path
from typing import Deque, Dict, Iterable, List, Optional, Set, Tuple


LOCK_ACTIONS = {"LOCK", "REQUEST", "REQ", "ACQUIRE"}
UNLOCK_ACTIONS = {"UNLOCK", "RELEASE", "REL"}


@dataclass(frozen=True)
class Event:
    timestamp: int
    process_id: str
    action: str
    resource_id: str
    duration_ms: Optional[int] = None


@dataclass
class DetectionResult:
    has_deadlock: bool
    cycle: List[str]


def parse_nullable_int(value: object) -> Optional[int]:
    text = "" if value is None else str(value).strip()
    if not text or text.lower() == "null":
        return None
    return int(float(text))


def read_events(csv_path: Path) -> List[Event]:
    events: List[Event] = []
    with csv_path.open(newline="") as file:
        reader = csv.DictReader(file)
        for row in reader:
            timestamp = parse_nullable_int(row.get("timestamp", row.get("time")))
            process_id = row.get("thread_id", row.get("process_id", "")).strip()
            action = row.get("action", "").strip().upper()
            resource_id = row.get("target_resource", row.get("resource_id", "")).strip()
            duration_ms = parse_nullable_int(row.get("duration_ms"))

            if timestamp is None or not process_id or not action or not resource_id:
                raise ValueError(f"Invalid row in {csv_path}: {row}")

            events.append(Event(timestamp, process_id, action, resource_id, duration_ms))

    return sorted(events, key=lambda event: event.timestamp)


def detect_cycle_dfs(graph: Dict[str, Set[str]]) -> DetectionResult:
    """Return one cycle if the directed wait-for graph has a cycle."""
    color: Dict[str, int] = {}
    stack: List[str] = []
    stack_index: Dict[str, int] = {}

    def visit(node: str) -> Optional[List[str]]:
        color[node] = 1
        stack_index[node] = len(stack)
        stack.append(node)

        for neighbor in graph.get(node, set()):
            state = color.get(neighbor, 0)
            if state == 0:
                cycle = visit(neighbor)
                if cycle:
                    return cycle
            elif state == 1:
                return stack[stack_index[neighbor] :] + [neighbor]

        stack.pop()
        stack_index.pop(node, None)
        color[node] = 2
        return None

    nodes = set(graph)
    for neighbors in graph.values():
        nodes.update(neighbors)

    for node in sorted(nodes):
        if color.get(node, 0) == 0:
            cycle = visit(node)
            if cycle:
                return DetectionResult(True, cycle)

    return DetectionResult(False, [])


def detect_cycle_bfs(graph: Dict[str, Set[str]]) -> DetectionResult:
    """Detect cycle with Kahn's BFS topological pruning."""
    nodes = set(graph)
    indegree = defaultdict(int)
    for source, neighbors in graph.items():
        nodes.add(source)
        for target in neighbors:
            nodes.add(target)
            indegree[target] += 1

    queue = deque(node for node in nodes if indegree[node] == 0)
    removed = 0

    while queue:
        node = queue.popleft()
        removed += 1
        for neighbor in graph.get(node, set()):
            indegree[neighbor] -= 1
            if indegree[neighbor] == 0:
                queue.append(neighbor)

    if removed == len(nodes):
        return DetectionResult(False, [])

    cycle_nodes = sorted(node for node in nodes if indegree[node] > 0)
    return DetectionResult(True, cycle_nodes)


class LockSimulator:
    def __init__(self) -> None:
        self.resource_owner: Dict[str, str] = {}
        self.resource_expiry: Dict[str, int] = {}
        self.wait_queues: Dict[str, Deque[str]] = defaultdict(deque)
        self.waiting_for: Dict[str, str] = {}
        self.process_holds: Dict[str, Set[str]] = defaultdict(set)

    def release_expired_locks(self, now: int) -> None:
        expired = [
            resource_id
            for resource_id, expiry in self.resource_expiry.items()
            if expiry <= now
        ]
        for resource_id in sorted(expired):
            owner = self.resource_owner.get(resource_id)
            if owner:
                self.release(owner, resource_id, now)

    def apply_event(self, event: Event) -> None:
        self.release_expired_locks(event.timestamp)

        if event.action in LOCK_ACTIONS:
            self.lock(event.process_id, event.resource_id, event.timestamp, event.duration_ms)
        elif event.action in UNLOCK_ACTIONS:
            self.release(event.process_id, event.resource_id, event.timestamp)
        else:
            raise ValueError(f"Unsupported action: {event.action}")

    def lock(
        self,
        process_id: str,
        resource_id: str,
        now: int,
        duration_ms: Optional[int],
    ) -> None:
        owner = self.resource_owner.get(resource_id)
        if owner is None or owner == process_id:
            self._grant(process_id, resource_id, now, duration_ms)
            return

        if self.waiting_for.get(process_id) != resource_id:
            self.waiting_for[process_id] = resource_id
            if process_id not in self.wait_queues[resource_id]:
                self.wait_queues[resource_id].append(process_id)

    def release(self, process_id: str, resource_id: str, now: int) -> None:
        if self.resource_owner.get(resource_id) != process_id:
            return

        self.resource_owner.pop(resource_id, None)
        self.resource_expiry.pop(resource_id, None)
        self.process_holds[process_id].discard(resource_id)
        self._grant_next_waiter(resource_id, now)

    def _grant(
        self,
        process_id: str,
        resource_id: str,
        now: int,
        duration_ms: Optional[int],
    ) -> None:
        self.resource_owner[resource_id] = process_id
        self.process_holds[process_id].add(resource_id)
        self.waiting_for.pop(process_id, None)
        if duration_ms is None:
            self.resource_expiry.pop(resource_id, None)
        else:
            self.resource_expiry[resource_id] = now + duration_ms

    def _grant_next_waiter(self, resource_id: str, now: int) -> None:
        while self.wait_queues[resource_id]:
            next_process = self.wait_queues[resource_id].popleft()
            if self.waiting_for.get(next_process) == resource_id:
                self._grant(next_process, resource_id, now, None)
                break

    def wait_for_graph(self) -> Dict[str, Set[str]]:
        graph: Dict[str, Set[str]] = defaultdict(set)
        for waiting_process, resource_id in self.waiting_for.items():
            owner = self.resource_owner.get(resource_id)
            if owner and owner != waiting_process:
                graph[waiting_process].add(owner)
        return dict(graph)


def graph_size(graph: Dict[str, Set[str]]) -> Tuple[int, int]:
    nodes = set(graph)
    edges = 0
    for neighbors in graph.values():
        nodes.update(neighbors)
        edges += len(neighbors)
    return len(nodes), edges


def choose_detector(name: str):
    if name == "dfs":
        return detect_cycle_dfs
    if name == "bfs":
        return detect_cycle_bfs
    raise ValueError(f"Unknown detector: {name}")


def simulate(
    events: Iterable[Event],
    detection_interval: int,
    detector_name: str,
) -> Dict[str, object]:
    simulation_start = time.perf_counter()
    simulator = LockSimulator()
    detector = choose_detector(detector_name)

    detection_frequency = 0
    total_detection_time_seconds = 0.0
    total_graph_build_time_seconds = 0.0
    first_actual_deadlock_time: Optional[int] = None
    first_detected_deadlock_time: Optional[int] = None
    last_cycle: List[str] = []
    max_nodes = 0
    max_edges = 0
    total_nodes = 0
    total_edges = 0
    event_count = 0

    for event_count, event in enumerate(events, start=1):
        simulator.apply_event(event)
        graph_start = time.perf_counter()
        graph = simulator.wait_for_graph()
        total_graph_build_time_seconds += time.perf_counter() - graph_start
        node_count, edge_count = graph_size(graph)
        max_nodes = max(max_nodes, node_count)
        max_edges = max(max_edges, edge_count)
        total_nodes += node_count
        total_edges += edge_count

        actual = detect_cycle_dfs(graph)
        if actual.has_deadlock and first_actual_deadlock_time is None:
            first_actual_deadlock_time = event.timestamp

        should_detect = detection_interval <= 1 or event_count % detection_interval == 0
        if should_detect:
            start = time.perf_counter()
            detected = detector(graph)
            total_detection_time_seconds += time.perf_counter() - start
            detection_frequency += 1

            if detected.has_deadlock:
                last_cycle = detected.cycle
                if first_detected_deadlock_time is None:
                    first_detected_deadlock_time = event.timestamp

    if event_count and event_count % detection_interval != 0:
        graph_start = time.perf_counter()
        graph = simulator.wait_for_graph()
        total_graph_build_time_seconds += time.perf_counter() - graph_start
        node_count, edge_count = graph_size(graph)
        max_nodes = max(max_nodes, node_count)
        max_edges = max(max_edges, edge_count)
        total_nodes += node_count
        total_edges += edge_count
        start = time.perf_counter()
        detected = detector(graph)
        total_detection_time_seconds += time.perf_counter() - start
        detection_frequency += 1
        if detected.has_deadlock:
            last_cycle = detected.cycle
            if first_detected_deadlock_time is None:
                first_detected_deadlock_time = event.timestamp

    if first_actual_deadlock_time is not None and first_detected_deadlock_time is not None:
        detection_latency = first_detected_deadlock_time - first_actual_deadlock_time
    else:
        detection_latency = None

    avg_detection_time_seconds = (
        total_detection_time_seconds / detection_frequency
        if detection_frequency
        else 0.0
    )
    avg_graph_build_time_seconds = (
        total_graph_build_time_seconds / event_count if event_count else 0.0
    )
    graph_samples = event_count + (1 if event_count and event_count % detection_interval != 0 else 0)
    avg_nodes = total_nodes / graph_samples if graph_samples else 0.0
    avg_edges = total_edges / graph_samples if graph_samples else 0.0
    total_simulation_time_seconds = time.perf_counter() - simulation_start

    return {
        "events": event_count,
        "detector": detector_name,
        "detection_interval": detection_interval,
        "detection_frequency": detection_frequency,
        "avg_detection_time_seconds": avg_detection_time_seconds,
        "total_detection_time_seconds": total_detection_time_seconds,
        "avg_graph_build_time_seconds": avg_graph_build_time_seconds,
        "total_graph_build_time_seconds": total_graph_build_time_seconds,
        "total_simulation_time_seconds": total_simulation_time_seconds,
        "detection_overhead": avg_detection_time_seconds * detection_frequency,
        "first_actual_deadlock_time": first_actual_deadlock_time,
        "first_detected_deadlock_time": first_detected_deadlock_time,
        "detection_latency": detection_latency,
        "deadlock_detected": first_detected_deadlock_time is not None,
        "cycle": " -> ".join(last_cycle) if last_cycle else "",
        "cycle_length": max(0, len(last_cycle) - 1),
        "avg_wait_for_nodes": avg_nodes,
        "avg_wait_for_edges": avg_edges,
        "max_wait_for_nodes": max_nodes,
        "max_wait_for_edges": max_edges,
    }


def print_result(result: Dict[str, object]) -> None:
    for key, value in result.items():
        if key.endswith("_seconds") and isinstance(value, float):
            print(f"{key}: {value:.9f}")
        elif key == "detection_overhead" and isinstance(value, float):
            print(f"{key}: {value:.9f}")
        else:
            print(f"{key}: {value}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Simulate deadlock detection with a Wait-for Graph."
    )
    parser.add_argument("dataset", type=Path, help="Path to CSV dataset")
    parser.add_argument(
        "-k",
        "--detection-interval",
        type=int,
        default=1,
        help="Run detection every K events; K=1 means every event.",
    )
    parser.add_argument(
        "--algorithm",
        choices=["dfs", "bfs"],
        default="dfs",
        help="Cycle detection algorithm.",
    )
    args = parser.parse_args()

    if args.detection_interval < 1:
        raise SystemExit("detection interval must be >= 1")

    events = read_events(args.dataset)
    result = simulate(events, args.detection_interval, args.algorithm)
    print_result(result)


if __name__ == "__main__":
    main()
