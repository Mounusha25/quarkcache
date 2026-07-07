"""
QuarkCache FastAPI backend.

Run with:
    uvicorn app.main:app --reload --port 8000
"""

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.routers import health, keys, metrics, benchmarks, ai

app = FastAPI(
    title="QuarkCache API",
    description=(
        "REST API for the QuarkCache in-memory key-value store. "
        "Bridges the Next.js dashboard and the C++ TCP cache server."
    ),
    version="1.0.0",
    docs_url="/docs",
    redoc_url="/redoc",
)

# Allow the Next.js dev server to call this API without CORS errors.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:3000"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(health.router)
app.include_router(keys.router)
app.include_router(metrics.router)
app.include_router(benchmarks.router)
app.include_router(ai.router)


@app.get("/", include_in_schema=False)
async def root():
    return {"message": "QuarkCache API — visit /docs for interactive documentation."}
