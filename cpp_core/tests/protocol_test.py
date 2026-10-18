#!/usr/bin/env python3
"""Black-box protocol checks against a real QuarkCache server process."""

from __future__ import annotations

import json
import os
import signal
import socket
import subprocess
import sys
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


def main() -> int:
    binary = Path(sys.argv[1]).resolve()
    port = available_port()
    environment = os.environ.copy()
    environment.update({"QUARKCACHE_BIND_HOST": "127.0.0.1", "QUARKCACHE_PORT": str(port)})
    process = subprocess.Popen(
        [str(binary)],
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
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

            assert exchange(connection, "SET expiring value 0") == "OK\r\n"
            assert exchange(connection, "GET expiring") == "NOT_FOUND\r\n"

            connection.sendall(b"SET partial")
            time.sleep(0.02)
            connection.sendall(b" value\r\n")
            assert connection.recv(4096) == b"OK\r\n"
            assert exchange(connection, "KEYS", terminator=b"END\r\n").endswith("END\r\n")
    finally:
        if process.poll() is None:
            process.send_signal(signal.SIGTERM)
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)

    print("protocol integration tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
