from fastapi import APIRouter, HTTPException, status
from app.services import cache_client, benchmark_service
from app.models.schemas import BenchmarkRequest, BenchmarkResponse

router = APIRouter(prefix="/benchmarks", tags=["Benchmarks"])


@router.post("/run", response_model=BenchmarkResponse, summary="Run benchmark")
async def run_benchmark(body: BenchmarkRequest) -> BenchmarkResponse:
    """
    Execute a configurable benchmark against the cache server.

    - **num_ops**: total operations to perform (1 – 500,000)
    - **read_ratio**: fraction of operations that are GETs (0.0 – 1.0)
    - **value_size**: bytes per stored value
    - **key_space**: number of distinct keys to use
    """
    try:
        return await benchmark_service.run_benchmark(body)
    except cache_client.CacheClientError as exc:
        raise HTTPException(status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                            detail=str(exc))
