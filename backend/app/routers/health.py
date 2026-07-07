from fastapi import APIRouter
from app.services import cache_client
from app.models.schemas import HealthResponse

router = APIRouter(prefix="/health", tags=["Health"])

VERSION = "1.0.0"


@router.get("", response_model=HealthResponse, summary="Health check")
async def health_check() -> HealthResponse:
    """Returns API status and whether the C++ cache server is reachable."""
    reachable = await cache_client.is_reachable()
    return HealthResponse(
        status="ok",
        version=VERSION,
        cpp_server_reachable=reachable,
    )
