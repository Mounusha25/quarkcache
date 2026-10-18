"""Operational maintenance endpoints."""

from fastapi import APIRouter, HTTPException, status

from app.models.schemas import CompactionResponse
from app.services import cache_client

router = APIRouter(prefix="/maintenance", tags=["Maintenance"])


@router.post(
    "/compact",
    response_model=CompactionResponse,
    summary="Compact the append-only file",
)
async def compact_aof() -> CompactionResponse:
    """Rewrite persistence atomically using one record per live cache key."""
    try:
        result = await cache_client.compact_aof()
        return CompactionResponse(**result)
    except cache_client.CacheClientError as exc:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail=str(exc),
        ) from exc
