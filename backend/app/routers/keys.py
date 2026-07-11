from fastapi import APIRouter, HTTPException, status
from app.services import cache_client
from app.models.schemas import (
    SetKeyRequest,
    KeyResponse,
    DeleteResponse,
    KeyListResponse,
    KeyListItem,
)

router = APIRouter(prefix="/keys", tags=["Keys"])


@router.get("", response_model=KeyListResponse, summary="List all keys")
async def list_keys() -> KeyListResponse:
    """Return all live keys currently stored in the cache."""
    try:
        raw = await cache_client.list_keys()
    except cache_client.CacheClientError as exc:
        raise HTTPException(status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                            detail=str(exc))
    items = [
        KeyListItem(key=e["key"], value=e["value"], ttl_remaining=e["ttl_remaining"])
        for e in raw
    ]
    return KeyListResponse(keys=items, count=len(items))


@router.post("", response_model=KeyResponse, status_code=status.HTTP_201_CREATED,
             summary="Set a key")
async def set_key(body: SetKeyRequest) -> KeyResponse:
    """Store a key-value pair with an optional TTL."""
    try:
        await cache_client.set_key(body.key, body.value, body.ttl)
    except cache_client.CacheClientError as exc:
        raise HTTPException(status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                            detail=str(exc))
    return KeyResponse(key=body.key, value=body.value, ttl_remaining=body.ttl)


@router.get("/{key}", response_model=KeyResponse, summary="Get a key")
async def get_key(key: str) -> KeyResponse:
    """Retrieve the value for a given key."""
    try:
        value = await cache_client.get_key(key)
    except cache_client.CacheClientError as exc:
        raise HTTPException(status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                            detail=str(exc))
    if value is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND,
                            detail=f"Key '{key}' not found")
    return KeyResponse(key=key, value=value, ttl_remaining=None)


@router.delete("/{key}", response_model=DeleteResponse, summary="Delete a key")
async def delete_key(key: str) -> DeleteResponse:
    """Delete a key from the cache."""
    try:
        deleted = await cache_client.delete_key(key)
    except cache_client.CacheClientError as exc:
        raise HTTPException(status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                            detail=str(exc))
    return DeleteResponse(key=key, deleted=deleted)
