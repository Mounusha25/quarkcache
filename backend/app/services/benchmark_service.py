"""
Benchmark service: runs a configurable mix of SET/GET operations against
the C++ cache server and computes latency percentiles.
"""

from __future__ import annotations

import asyncio
import random
import string
import time
import statistics
from typing import List

from app.services import cache_client
from app.models.schemas import BenchmarkRequest, BenchmarkResponse, LatencyPercentiles


def _random_value(size: int) -> str:
    """Generate a random ASCII string of *size* characters."""
    return "".join(random.choices(string.ascii_letters + string.digits, k=size))


async def _timed_op(key: str, value: str, is_read: bool) -> tuple[float, bool]:
    """Perform one SET or GET and return (latency_ms, error_occurred)."""
    t0 = time.perf_counter()
    error = False
    try:
        if is_read:
            await cache_client.get_key(key)
        else:
            await cache_client.set_key(key, value)
    except cache_client.CacheClientError:
        error = True
    latency_ms = (time.perf_counter() - t0) * 1000
    return latency_ms, error


async def run_benchmark(req: BenchmarkRequest) -> BenchmarkResponse:
    """
    Execute *req.num_ops* operations against the cache server.

    Operations are run concurrently in batches of 64 to avoid overwhelming
    the server while still exercising its concurrency.
    """
    num_read = int(req.num_ops * req.read_ratio)
    num_write = req.num_ops - num_read

    # Pre-generate a key space and random values.
    keys = [f"bench:{i}" for i in range(req.key_space)]
    values = [_random_value(req.value_size) for _ in range(min(req.key_space, 200))]

    ops: List[tuple[str, str, bool]] = []
    for _ in range(num_write):
        k = random.choice(keys)
        v = random.choice(values)
        ops.append((k, v, False))
    for _ in range(num_read):
        k = random.choice(keys)
        ops.append((k, "", True))

    random.shuffle(ops)

    latencies: List[float] = []
    errors = 0
    batch_size = 64

    t_start = time.perf_counter()
    for i in range(0, len(ops), batch_size):
        batch = ops[i : i + batch_size]
        results = await asyncio.gather(
            *[_timed_op(k, v, is_read) for k, v, is_read in batch],
            return_exceptions=False,
        )
        for lat, err in results:
            latencies.append(lat)
            if err:
                errors += 1

    total_ms = (time.perf_counter() - t_start) * 1000

    latencies_sorted = sorted(latencies)
    n = len(latencies_sorted)

    def pct(p: float) -> float:
        if not latencies_sorted:
            return 0.0
        idx = int(p / 100.0 * n)
        return round(latencies_sorted[min(idx, n - 1)], 3)

    return BenchmarkResponse(
        num_ops=req.num_ops,
        total_time_ms=round(total_ms, 2),
        ops_per_sec=round(req.num_ops / (total_ms / 1000), 2) if total_ms > 0 else 0,
        latency=LatencyPercentiles(
            p50_ms=pct(50),
            p95_ms=pct(95),
            p99_ms=pct(99),
            min_ms=round(min(latencies_sorted), 3) if latencies_sorted else 0,
            max_ms=round(max(latencies_sorted), 3) if latencies_sorted else 0,
        ),
        errors=errors,
    )
