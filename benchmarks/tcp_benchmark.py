#!/usr/bin/env python3
"""Concurrent black-box benchmark for the QuarkCache TCP protocol."""

from __future__ import annotations

import argparse
import json
import os
import signal
import socket
import statistics
import subprocess
import threading
import time
from pathlib import Path


def available_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def wait_until_ready(process: subprocess.Popen[str], port: int) -> None:
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("QuarkCache exited before the benchmark started")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                return
        except OSError:
            time.sleep(0.05)
    raise TimeoutError("QuarkCache did not start within five seconds")


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    index = max(0, min(len(ordered) - 1, int(fraction * len(ordered)) - 1))
    return ordered[index]


def run_client(
    client_id: int,
    operations: int,
    value: str,
    port: int,
    latencies_ms: list[float],
    errors: list[str],
    lock: threading.Lock,
) -> None:
    local_latencies = []
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=5) as connection:
            stream = connection.makefile("rb")
            for index in range(operations):
                key = f"benchmark:{client_id}:{index // 2}"
                command = f"SET {key} {value}" if index % 2 == 0 else f"GET {key}"
                expected = b"OK\r\n" if index % 2 == 0 else f"VALUE {value}\r\n".encode()
                started = time.perf_counter_ns()
                connection.sendall(command.encode() + b"\r\n")
                response = stream.readline()
                local_latencies.append((time.perf_counter_ns() - started) / 1_000_000)
                if response != expected:
                    raise RuntimeError(f"unexpected response to {command!r}: {response!r}")
    except Exception as exc:  # benchmark must report worker failures to the caller
        with lock:
            errors.append(f"client {client_id}: {exc}")
    finally:
        with lock:
            latencies_ms.extend(local_latencies)


def benchmark(binary: Path, clients: int, operations: int, value_size: int) -> dict:
    port = available_port()
    environment = os.environ.copy()
    environment.update({"QUARKCACHE_BIND_HOST": "127.0.0.1", "QUARKCACHE_PORT": str(port)})
    process = subprocess.Popen(
        [str(binary.resolve())],
        env=environment,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
        text=True,
    )
    latencies_ms: list[float] = []
    errors: list[str] = []
    lock = threading.Lock()
    try:
        wait_until_ready(process, port)
        value = "x" * value_size
        threads = [
            threading.Thread(
                target=run_client,
                args=(client_id, operations, value, port, latencies_ms, errors, lock),
            )
            for client_id in range(clients)
        ]
        started = time.perf_counter()
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        elapsed = time.perf_counter() - started
    finally:
        if process.poll() is None:
            process.send_signal(signal.SIGTERM)
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)

    if errors:
        raise RuntimeError("; ".join(errors))
    completed = len(latencies_ms)
    return {
        "clients": clients,
        "operations_per_client": operations,
        "operations_completed": completed,
        "value_bytes": value_size,
        "duration_seconds": round(elapsed, 4),
        "throughput_ops_per_second": round(completed / elapsed, 2),
        "latency_ms": {
            "mean": round(statistics.mean(latencies_ms), 4),
            "p50": round(percentile(latencies_ms, 0.50), 4),
            "p95": round(percentile(latencies_ms, 0.95), 4),
            "p99": round(percentile(latencies_ms, 0.99), 4),
            "max": round(max(latencies_ms), 4),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server-binary", type=Path, default=Path("build/quarkcache_server"))
    parser.add_argument("--clients", type=int, default=8)
    parser.add_argument("--operations", type=int, default=5_000)
    parser.add_argument("--value-size", type=int, default=64)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.clients < 1 or args.operations < 1 or args.value_size < 1:
        parser.error("clients, operations, and value-size must be positive")

    result = benchmark(args.server_binary, args.clients, args.operations, args.value_size)
    rendered = json.dumps(result, indent=2)
    print(rendered)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
