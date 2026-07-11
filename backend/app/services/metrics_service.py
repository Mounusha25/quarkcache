"""
Metrics service: wraps the cache_client stats call and adds derived metrics
like hit_ratio and ops_per_sec (computed with a rolling window).
"""

from __future__ import annotations

import time
import asyncio
from collections import deque
from typing import Deque, Tuple

from app.services import cache_client
from app.models.schemas import MetricsResponse

# Memory limit assumed for the C++ server (100 MB by default).
_MEMORY_LIMIT_BYTES = 100 * 1024 * 1024

# Rolling window for ops/sec calculation (circular buffer of (timestamp, count)).
_request_history: Deque[Tuple[float, int]] = deque(maxlen=60)
_last_total: int = 0


async def get_metrics() -> MetricsResponse:
    global _last_total

    raw = await cache_client.get_stats()

    total   = raw.get("total_requests", 0)
    hits    = raw.get("hits", 0)
    misses  = raw.get("misses", 0)
    bytes_  = raw.get("bytes_used", 0)
    conns   = raw.get("active_connections", 0)

    hit_ratio = hits / total if total > 0 else 0.0

    # Track request delta over the last second for ops/sec.
    now = time.monotonic()
    delta = total - _last_total
    _last_total = total
    _request_history.append((now, delta))

    # Sum deltas within the last second.
    recent_ops = sum(
        count for ts, count in _request_history if now - ts <= 1.0
    )

    return MetricsResponse(
        total_requests=total,
        hits=hits,
        misses=misses,
        hit_ratio=round(hit_ratio, 4),
        ops_per_sec=float(recent_ops),
        memory_used_bytes=bytes_,
        memory_limit_bytes=_MEMORY_LIMIT_BYTES,
        active_connections=conns,
    )
