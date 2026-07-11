"""Pydantic request/response schemas shared across routers."""

from __future__ import annotations

from typing import Optional
from pydantic import BaseModel, Field


# --------------------------------------------------------------------------- #
#  Keys
# --------------------------------------------------------------------------- #

class SetKeyRequest(BaseModel):
    key: str = Field(..., min_length=1, description="Cache key")
    value: str = Field(..., description="String value to store")
    ttl: Optional[int] = Field(
        None, ge=1, description="Time-to-live in seconds (omit for no expiry)"
    )


class KeyResponse(BaseModel):
    key: str
    value: str
    ttl_remaining: Optional[int] = Field(
        None, description="Approximate seconds until expiry; null if no TTL"
    )


class DeleteResponse(BaseModel):
    key: str
    deleted: bool


class KeyListItem(BaseModel):
    key: str
    value: str
    ttl_remaining: Optional[int]


class KeyListResponse(BaseModel):
    keys: list[KeyListItem]
    count: int


# --------------------------------------------------------------------------- #
#  Metrics
# --------------------------------------------------------------------------- #

class MetricsResponse(BaseModel):
    total_requests: int
    hits: int
    misses: int
    hit_ratio: float = Field(..., description="hits / total_requests (0–1)")
    ops_per_sec: float = Field(..., description="Requests in the last second")
    memory_used_bytes: int
    memory_limit_bytes: int
    active_connections: int


# --------------------------------------------------------------------------- #
#  Benchmarks
# --------------------------------------------------------------------------- #

class BenchmarkRequest(BaseModel):
    num_ops: int = Field(1000, ge=1, le=500_000, description="Total operations")
    read_ratio: float = Field(
        0.8, ge=0.0, le=1.0,
        description="Fraction of operations that are GETs (remainder are SETs)"
    )
    value_size: int = Field(64, ge=1, le=4096, description="Bytes per value")
    key_space: int = Field(1000, ge=1, description="Number of distinct keys")


class LatencyPercentiles(BaseModel):
    p50_ms: float
    p95_ms: float
    p99_ms: float
    min_ms: float
    max_ms: float


class BenchmarkResponse(BaseModel):
    num_ops: int
    total_time_ms: float
    ops_per_sec: float
    latency: LatencyPercentiles
    errors: int


# --------------------------------------------------------------------------- #
#  Health
# --------------------------------------------------------------------------- #

class HealthResponse(BaseModel):
    status: str
    version: str
    cpp_server_reachable: bool
