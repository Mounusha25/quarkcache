from fastapi import APIRouter
from pydantic import BaseModel
from app.services import ai_service

router = APIRouter(prefix="/ai", tags=["AI"])


class ChatRequest(BaseModel):
    message: str
    history: list[dict] = []


class ChatResponse(BaseModel):
    reply: str
    ai_enhanced: bool


@router.get("/insights", summary="AI-powered cache insights")
async def get_insights():
    """
    Returns rule-based (always) + GPT-enhanced (if OPENAI_API_KEY set)
    insights about the current cache state.
    """
    return await ai_service.get_insights()


@router.post("/chat", response_model=ChatResponse, summary="Chat with AI about the cache")
async def chat(body: ChatRequest) -> ChatResponse:
    """
    Ask any question about cache performance, architecture, or configuration.
    Uses OpenAI GPT-4o-mini if OPENAI_API_KEY is configured, otherwise
    falls back to intelligent rule-based responses.
    """
    import os
    reply = await ai_service.chat(body.message, body.history)
    return ChatResponse(reply=reply, ai_enhanced=bool(os.getenv("OPENAI_API_KEY")))
