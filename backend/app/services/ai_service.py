"""
AI Insights service.

Rule-based analysis runs always (no API key needed).
If OPENAI_API_KEY is set in the environment, responses are enhanced via GPT.
"""

from __future__ import annotations

import os
import json
from typing import AsyncGenerator

from app.services import cache_client, metrics_service

# ── Optional OpenAI integration ────────────────────────────────────────────────
try:
    from openai import AsyncOpenAI
    _openai_client = AsyncOpenAI(api_key=os.getenv("OPENAI_API_KEY", ""))
    _HAS_OPENAI = bool(os.getenv("OPENAI_API_KEY"))
except ImportError:
    _HAS_OPENAI = False
    _openai_client = None  # type: ignore

_SYSTEM_PROMPT = """\
You are an expert Redis/cache engineer assistant embedded in QuarkCache — a
Redis-like in-memory key-value store. You help developers understand cache
performance, diagnose issues, and optimise configurations. Be concise, practical,
and technical. Answer in plain text (no markdown headers). Keep replies under
200 words unless the user asks for detail.\
"""


# ── Rule-based insights ────────────────────────────────────────────────────────

def _rule_insights(stats: dict) -> list[str]:
    """Return a list of human-readable insights based on metrics thresholds."""
    insights: list[str] = []
    total    = stats.get("total_requests", 0)
    hits     = stats.get("hits", 0)
    misses   = stats.get("misses", 0)
    bytes_   = stats.get("bytes_used", 0)
    conns    = stats.get("active_connections", 0)

    hit_ratio = hits / total if total > 0 else 0.0

    if total == 0:
        insights.append("No requests recorded yet. Start sending commands to see live metrics.")
        return insights

    if hit_ratio < 0.5:
        insights.append(
            f"Low hit ratio ({hit_ratio*100:.1f}%). Consider increasing TTL values or "
            "pre-warming the cache with common keys to reduce misses."
        )
    elif hit_ratio > 0.95:
        insights.append(
            f"Excellent hit ratio ({hit_ratio*100:.1f}%). The cache is serving most requests "
            "without touching the origin."
        )
    else:
        insights.append(
            f"Hit ratio is {hit_ratio*100:.1f}% — healthy, but there's room to improve "
            "with better cache key design."
        )

    if bytes_ > 80 * 1024 * 1024:
        insights.append(
            "Memory usage is above 80 MB. Consider reducing max_keys, lowering TTLs, "
            "or compressing values to avoid LRU thrashing."
        )

    if conns > 50:
        insights.append(
            f"{conns} active connections detected. If this is unexpected, check for "
            "connection leaks in your client code."
        )

    if misses > hits and total > 100:
        insights.append(
            "More misses than hits. Your workload may have a skewed key distribution — "
            "try increasing the key_space or warming hot keys at startup."
        )

    return insights


# ── Public API ─────────────────────────────────────────────────────────────────

async def get_insights() -> dict:
    """Return rule-based insights + current stats."""
    try:
        raw = await cache_client.get_stats()
    except cache_client.CacheClientError:
        raw = {}

    return {
        "insights": _rule_insights(raw),
        "stats": raw,
        "ai_enhanced": False,
    }


async def chat(message: str, history: list[dict]) -> str:
    """
    Answer a user question about the cache.
    Uses OpenAI GPT if configured, otherwise falls back to rule-based.
    """
    # Always pull fresh metrics to inject into context.
    try:
        raw = await cache_client.get_stats()
        context = f"Current cache stats: {json.dumps(raw)}"
    except cache_client.CacheClientError:
        context = "Cache server is currently unreachable."

    if _HAS_OPENAI and _openai_client:
        messages = [
            {"role": "system", "content": _SYSTEM_PROMPT + f"\n\n{context}"},
            *history[-10:],  # keep last 10 turns for context window efficiency
            {"role": "user", "content": message},
        ]
        resp = await _openai_client.chat.completions.create(
            model="gpt-4o-mini",
            messages=messages,  # type: ignore[arg-type]
            max_tokens=400,
            temperature=0.4,
        )
        return resp.choices[0].message.content or "No response."

    # ── Fallback: deterministic rule-based responses ───────────────────────────
    msg = message.lower()
    insights = _rule_insights(raw if "raw" in dir() else {})

    if any(w in msg for w in ["hit", "ratio", "miss"]):
        total = raw.get("total_requests", 0)
        hits  = raw.get("hits", 0)
        ratio = hits / total if total > 0 else 0
        return (
            f"Current hit ratio is {ratio*100:.1f}% ({hits:,} hits / {total:,} total). "
            + (insights[0] if insights else "")
        )

    if any(w in msg for w in ["memory", "mem", "bytes"]):
        b = raw.get("bytes_used", 0)
        return (
            f"Memory in use: {b / 1024:.1f} KB ({b:,} bytes). "
            "The C++ server uses an approximate byte counter based on key+value string lengths."
        )

    if any(w in msg for w in ["lru", "evict"]):
        return (
            "QuarkCache uses an O(1) LRU eviction policy implemented with a doubly-linked list "
            "and a hash map. When max_keys is reached, the least-recently-used key is evicted first. "
            "Keys with expired TTLs are also lazily evicted on access."
        )

    if any(w in msg for w in ["ttl", "expire", "expir"]):
        return (
            "TTL (time-to-live) is set per key using SET <key> <value> <ttl_seconds>. "
            "The C++ server checks expiry lazily on GET and also runs a background sweep every 5 seconds."
        )

    if any(w in msg for w in ["slow", "latency", "fast", "perf"]):
        return (
            "The C++ TCP server uses a non-blocking epoll/kqueue event loop. "
            "For maximum throughput, use pipeline-style batch commands, keep values small, "
            "and ensure the client connection pool is sized appropriately."
        )

    if any(w in msg for w in ["help", "what", "how"]):
        return (
            "I can answer questions about: hit/miss ratios, memory usage, LRU eviction, "
            "TTL configuration, performance tuning, and architecture. "
            "For AI-enhanced responses, set the OPENAI_API_KEY environment variable."
        )

    return (
        "I understand you're asking about the cache. "
        + (" ".join(insights) if insights else "The cache appears healthy.")
        + " For richer AI answers, set OPENAI_API_KEY in the backend environment."
    )
