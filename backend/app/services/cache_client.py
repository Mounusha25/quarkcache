"""
Async TCP client for the QuarkCache C++ server.

Protocol (line-delimited, \r\n terminated):
    SET <key> <value> [<ttl>]\r\n  →  OK | ERROR ...
    GET <key>\r\n                  →  VALUE <val> | NOT_FOUND | ERROR ...
    DEL <key>\r\n                  →  OK | NOT_FOUND | ERROR ...
    STATS\r\n                      →  STATS <json>
    KEYS\r\n                       →  <key> <value> <ttl|-1>\r\n ... END\r\n
"""

from __future__ import annotations

import asyncio
import json
import logging
from typing import Optional

import os

logger = logging.getLogger(__name__)

_HOST = os.getenv("QUARKCACHE_HOST", "127.0.0.1")
_PORT = int(os.getenv("QUARKCACHE_PORT", "9000"))
_TIMEOUT = 5.0  # seconds


class CacheClientError(Exception):
    """Raised when the cache server returns an error or is unreachable."""


async def _send_command(command: str) -> str:
    """Open a TCP connection, send one command, and return the full response."""
    try:
        reader, writer = await asyncio.wait_for(
            asyncio.open_connection(_HOST, _PORT), timeout=_TIMEOUT
        )
    except (ConnectionRefusedError, OSError) as exc:
        raise CacheClientError(f"Cannot connect to cache server: {exc}") from exc
    except asyncio.TimeoutError as exc:
        raise CacheClientError("Connection to cache server timed out") from exc

    try:
        wire = command if command.endswith("\r\n") else command + "\r\n"
        writer.write(wire.encode())
        await writer.drain()

        # For KEYS we read until the END sentinel; for everything else one line suffices.
        if command.strip().upper().startswith("KEYS"):
            lines: list[str] = []
            while True:
                line = await asyncio.wait_for(reader.readline(), timeout=_TIMEOUT)
                decoded = line.decode().rstrip("\r\n")
                if decoded == "END":
                    break
                lines.append(decoded)
            return "\n".join(lines)
        else:
            line = await asyncio.wait_for(reader.readline(), timeout=_TIMEOUT)
            return line.decode().rstrip("\r\n")
    except asyncio.TimeoutError as exc:
        raise CacheClientError("Cache server response timed out") from exc
    finally:
        writer.close()
        try:
            await writer.wait_closed()
        except Exception:
            pass


async def set_key(key: str, value: str, ttl: Optional[int] = None) -> None:
    """Store *key → value* with an optional TTL (seconds)."""
    cmd = f"SET {key} {value}"
    if ttl is not None:
        cmd += f" {ttl}"
    response = await _send_command(cmd)
    if not response.startswith("OK"):
        raise CacheClientError(f"SET failed: {response}")


async def get_key(key: str) -> Optional[str]:
    """Return the value for *key*, or None if not found / expired."""
    response = await _send_command(f"GET {key}")
    if response.startswith("VALUE "):
        return response[len("VALUE "):]
    if response == "NOT_FOUND":
        return None
    raise CacheClientError(f"GET failed: {response}")


async def delete_key(key: str) -> bool:
    """Delete *key*. Returns True if key existed."""
    response = await _send_command(f"DEL {key}")
    if response == "OK":
        return True
    if response == "NOT_FOUND":
        return False
    raise CacheClientError(f"DEL failed: {response}")


async def get_stats() -> dict:
    """Return the raw stats dict from the C++ server."""
    response = await _send_command("STATS")
    if not response.startswith("STATS "):
        raise CacheClientError(f"STATS failed: {response}")
    payload = response[len("STATS "):]
    try:
        return json.loads(payload)
    except json.JSONDecodeError as exc:
        raise CacheClientError(f"Could not parse STATS JSON: {exc}") from exc


async def list_keys() -> list[dict]:
    """Return all live keys as a list of {key, value, ttl_remaining} dicts."""
    response = await _send_command("KEYS")
    items: list[dict] = []
    for line in response.splitlines():
        parts = line.split(" ", 2)
        if len(parts) < 2:
            continue
        key = parts[0]
        value = parts[1] if len(parts) > 1 else ""
        ttl_str = parts[2] if len(parts) > 2 else "-1"
        try:
            ttl_val: Optional[int] = int(ttl_str)
            ttl_remaining = None if ttl_val < 0 else ttl_val
        except ValueError:
            ttl_remaining = None
        items.append({"key": key, "value": value, "ttl_remaining": ttl_remaining})
    return items


async def is_reachable() -> bool:
    """Return True if the C++ server is reachable."""
    try:
        await get_stats()
        return True
    except CacheClientError:
        return False
