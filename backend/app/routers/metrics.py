from fastapi import APIRouter, HTTPException, status
from app.services import cache_client, metrics_service
from app.models.schemas import MetricsResponse

router = APIRouter(prefix="/metrics", tags=["Metrics"])


@router.get("", response_model=MetricsResponse, summary="Cache metrics")
async def get_metrics() -> MetricsResponse:
    """
    Retrieve real-time cache metrics including ops/sec, hit/miss ratio,
    memory usage, and active connection count.
    """
    try:
        return await metrics_service.get_metrics()
    except cache_client.CacheClientError as exc:
        raise HTTPException(status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                            detail=str(exc))
