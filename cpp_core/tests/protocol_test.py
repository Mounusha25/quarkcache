#!/usr/bin/env python3
"""Black-box protocol checks against a real QuarkCache server process."""

from __future__ import annotations

import json
import os
import signal
import socket
import subprocess
import sys
import tempfile
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
            output = process.stdout.read() if process.stdout else ""
            raise RuntimeError(f"server exited before accepting connections: {output}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                return
        except OSError:
            time.sleep(0.05)
    raise TimeoutError("server did not start within five seconds")


def exchange(connection: socket.socket, command: str, *, terminator: bytes = b"\r\n") -> str:
    connection.sendall(command.encode() + b"\r\n")
    response = bytearray()
    while terminator not in response:
        chunk = connection.recv(4096)
        if not chunk:
            raise ConnectionError("server closed the connection")
        response.extend(chunk)
    return response.decode()


def launch(binary: Path, port: int, aof_path: Path) -> subprocess.Popen[str]:
    environment = os.environ.copy()
    environment.update(
        {
            "QUARKCACHE_BIND_HOST": "127.0.0.1",
            "QUARKCACHE_PORT": str(port),
            "QUARKCACHE_AOF_PATH": str(aof_path),
            "QUARKCACHE_FSYNC": "always",
        }
    )
    return subprocess.Popen(
        [str(binary)],
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )


def stop(process: subprocess.Popen[str], *, crash: bool = False) -> None:
    if process.poll() is not None:
        return
    if crash:
        process.kill()
    else:
        process.send_signal(signal.SIGTERM)
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)


def main() -> int:
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="quarkcache-protocol-") as directory:
        aof_path = Path(directory) / "appendonly.aof"
        port = available_port()
        process = launch(binary, port, aof_path)
        try:
            wait_until_ready(process, port)
            with socket.create_connection(("127.0.0.1", port), timeout=2) as connection:
                assert exchange(connection, "SET greeting hello") == "OK\r\n"
                assert exchange(connection, "GET greeting") == "VALUE hello\r\n"
                assert exchange(connection, "DEL greeting") == "OK\r\n"
                assert exchange(connection, "GET greeting") == "NOT_FOUND\r\n"
                assert exchange(connection, "SET") == "ERROR SET <key> <value> [<ttl>]\r\n"
                assert exchange(connection, "SET key value invalid") == "ERROR invalid TTL\r\n"
                assert exchange(connection, "NOPE") == "ERROR unknown command 'NOPE'\r\n"
                stats_line = exchange(connection, "STATS")
                stats = json.loads(stats_line.removeprefix("STATS ").strip())
                assert stats["total_requests"] == 4
                assert stats["hits"] == 1
                assert stats["misses"] == 1
                assert stats["active_connections"] == 1
                assert stats["aof_enabled"] is True

                assert exchange(connection, "SET expiring value 0") == "OK\r\n"
                assert exchange(connection, "GET expiring") == "NOT_FOUND\r\n"

                connection.sendall(b"SET partial")
                time.sleep(0.02)
                connection.sendall(b" value\r\n")
                assert connection.recv(4096) == b"OK\r\n"
                assert exchange(connection, "KEYS", terminator=b"END\r\n").endswith(
                    "END\r\n"
                )
                assert exchange(connection, "SET durable recovered") == "OK\r\n"
                assert exchange(connection, "SET ttl-key alive 30") == "OK\r\n"
                assert exchange(connection, "SET removed value") == "OK\r\n"
                assert exchange(connection, "DEL removed") == "OK\r\n"
                for version in range(20):
                    assert exchange(connection, f"SET churn value-{version}") == "OK\r\n"
                assert exchange(connection, "DEL churn") == "OK\r\n"

                before_line = exchange(connection, "STATS")
                before = json.loads(before_line.removeprefix("STATS ").strip())
                compact_line = exchange(connection, "COMPACT")
                compact = json.loads(compact_line.removeprefix("COMPACT ").strip())
                assert compact["before_bytes"] == before["aof_size_bytes"]
                assert compact["after_bytes"] < compact["before_bytes"]
                assert compact["reclaimed_bytes"] > 0
                assert compact["records"] == 3

                after_line = exchange(connection, "STATS")
                after = json.loads(after_line.removeprefix("STATS ").strip())
                assert after["aof_compactions"] == 1
                assert after["aof_reclaimed_bytes"] == compact["reclaimed_bytes"]
        finally:
            stop(process, crash=True)

        recovery_port = available_port()
        recovered = launch(binary, recovery_port, aof_path)
        try:
            wait_until_ready(recovered, recovery_port)
            with socket.create_connection(("127.0.0.1", recovery_port), timeout=2) as connection:
                assert exchange(connection, "GET durable") == "VALUE recovered\r\n"
                assert exchange(connection, "GET ttl-key") == "VALUE alive\r\n"
                assert exchange(connection, "GET removed") == "NOT_FOUND\r\n"
                assert exchange(connection, "GET expiring") == "NOT_FOUND\r\n"
                stats_line = exchange(connection, "STATS")
                stats = json.loads(stats_line.removeprefix("STATS ").strip())
                assert stats["aof_recovered_records"] == 3
                assert stats["aof_truncated_bytes"] == 0
        finally:
            stop(recovered)

    print("protocol integration tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
