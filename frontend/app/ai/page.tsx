"use client";

import React, { useEffect, useRef, useState } from "react";
import {
  Alert, Box, Card, CardContent, Chip, CircularProgress,
  Divider, Grid, IconButton, InputAdornment, TextField, Typography,
} from "@mui/material";
import AutoAwesomeIcon from "@mui/icons-material/AutoAwesome";
import SendIcon from "@mui/icons-material/Send";
import LightbulbIcon from "@mui/icons-material/Lightbulb";
import api from "@/lib/apiClient";

interface Message { role: "user" | "assistant"; content: string; enhanced?: boolean }
interface InsightsData { insights: string[]; ai_enhanced: boolean }

const STARTERS = [
  "What's my hit ratio and how can I improve it?",
  "Why is memory usage increasing?",
  "How does LRU eviction work?",
  "How do I tune TTL for best performance?",
  "What does ops/sec tell me about my workload?",
  "Explain the C++ epoll server architecture",
];

export default function AIPage() {
  const [messages, setMessages]   = useState<Message[]>([]);
  const [input, setInput]         = useState("");
  const [loading, setLoading]     = useState(false);
  const [insights, setInsights]   = useState<InsightsData | null>(null);
  const [insightErr, setInsightErr] = useState<string | null>(null);
  const bottomRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    api.get<InsightsData>("/ai/insights")
      .then(r => setInsights(r.data))
      .catch(() => setInsightErr("Could not load insights — is the cache server running?"));
  }, []);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [messages]);

  const send = async (text?: string) => {
    const msg = (text ?? input).trim();
    if (!msg || loading) return;
    setInput("");

    const userMsg: Message = { role: "user", content: msg };
    setMessages(prev => [...prev, userMsg]);
    setLoading(true);

    try {
      const history = messages.map(m => ({ role: m.role, content: m.content }));
      const res = await api.post<{ reply: string; ai_enhanced: boolean }>("/ai/chat", {
        message: msg,
        history,
      });
      setMessages(prev => [...prev, {
        role: "assistant",
        content: res.data.reply,
        enhanced: res.data.ai_enhanced,
      }]);
    } catch {
      setMessages(prev => [...prev, { role: "assistant", content: "Sorry, I could not connect to the AI service. Make sure the FastAPI backend is running." }]);
    } finally {
      setLoading(false);
    }
  };

  return (
    <Box>
      <Box sx={{ mb: 3, display: "flex", alignItems: "center", gap: 1.5 }}>
        <AutoAwesomeIcon sx={{ color: "primary.main" }} />
        <Box>
          <Typography variant="h5" sx={{ fontWeight: 700 }}>AI Insights</Typography>
          <Typography variant="body2" color="text.secondary">
            Ask anything about your cache — powered by rule-based analysis + optional GPT-4o
          </Typography>
        </Box>
        <Chip label="GPT-4o-mini" size="small"
          sx={{ ml: "auto", bgcolor: "rgba(124,58,237,0.15)", color: "primary.main", border: "1px solid rgba(124,58,237,0.3)" }} />
      </Box>

      <Grid container spacing={2}>
        {/* Chat panel */}
        <Grid size={{ xs: 12, md: 8 }}>
          <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3 }}>
            <CardContent sx={{ display: "flex", flexDirection: "column", height: 500, pb: "12px !important" }}>
              {/* Messages */}
              <Box sx={{ flexGrow: 1, overflowY: "auto", mb: 2, pr: 1 }}>
                {messages.length === 0 && (
                  <Box sx={{ textAlign: "center", mt: 8 }}>
                    <AutoAwesomeIcon sx={{ fontSize: 48, color: "primary.main", opacity: 0.5, mb: 2 }} />
                    <Typography color="text.secondary" sx={{ mb: 1 }}>
                      Ask me anything about QuarkCache
                    </Typography>
                    <Typography variant="caption" color="text.disabled">
                      Set OPENAI_API_KEY in the backend for GPT-enhanced answers
                    </Typography>
                  </Box>
                )}
                {messages.map((m, i) => (
                  <Box key={i} sx={{ mb: 2, display: "flex", flexDirection: "column",
                    alignItems: m.role === "user" ? "flex-end" : "flex-start" }}>
                    <Box sx={{
                      maxWidth: "85%", px: 2, py: 1.5, borderRadius: 2,
                      bgcolor: m.role === "user" ? "primary.main" : "rgba(255,255,255,0.05)",
                      color: m.role === "user" ? "primary.contrastText" : "text.primary",
                    }}>
                      <Typography variant="body2" sx={{ lineHeight: 1.7, whiteSpace: "pre-wrap" }}>
                        {m.content}
                      </Typography>
                    </Box>
                    {m.role === "assistant" && m.enhanced && (
                      <Chip label="GPT-4o" size="small" sx={{ mt: 0.5, height: 18, fontSize: "0.65rem",
                        bgcolor: "rgba(124,58,237,0.12)", color: "primary.main" }} />
                    )}
                  </Box>
                ))}
                {loading && (
                  <Box sx={{ display: "flex", alignItems: "center", gap: 1, mb: 2 }}>
                    <CircularProgress size={16} />
                    <Typography variant="caption" color="text.secondary">Thinking…</Typography>
                  </Box>
                )}
                <div ref={bottomRef} />
              </Box>

              <Divider sx={{ borderColor: "rgba(255,255,255,0.06)", mb: 1.5 }} />

              {/* Input */}
              <TextField
                value={input}
                onChange={e => setInput(e.target.value)}
                onKeyDown={e => e.key === "Enter" && !e.shiftKey && send()}
                placeholder="Ask about hit ratio, memory, LRU, TTL, performance…"
                fullWidth size="small" multiline maxRows={3}
                sx={{ "& .MuiOutlinedInput-root": { bgcolor: "rgba(255,255,255,0.03)" } }}
                slotProps={{
                  input: {
                    endAdornment: (
                      <InputAdornment position="end">
                        <IconButton size="small" onClick={() => send()} disabled={!input.trim() || loading}>
                          <SendIcon fontSize="small" />
                        </IconButton>
                      </InputAdornment>
                    ),
                  },
                }}
              />
            </CardContent>
          </Card>
        </Grid>

        {/* Insights panel */}
        <Grid size={{ xs: 12, md: 4 }}>
          <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3, mb: 2 }}>
            <CardContent>
              <Box sx={{ display: "flex", alignItems: "center", gap: 1, mb: 2 }}>
                <LightbulbIcon sx={{ fontSize: 18, color: "#f59e0b" }} />
                <Typography variant="subtitle2" sx={{ fontWeight: 600 }}>Live Insights</Typography>
              </Box>
              {insightErr && <Alert severity="error" sx={{ fontSize: "0.78rem" }}>{insightErr}</Alert>}
              {!insights && !insightErr && <CircularProgress size={20} />}
              {insights?.insights.map((ins, i) => (
                <Box key={i} sx={{ mb: 1.5, p: 1.5, bgcolor: "rgba(245,158,11,0.07)",
                  borderRadius: 2, borderLeft: "3px solid #f59e0b" }}>
                  <Typography variant="caption" sx={{ lineHeight: 1.6 }}>{ins}</Typography>
                </Box>
              ))}
            </CardContent>
          </Card>

          {/* Starter prompts */}
          <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3 }}>
            <CardContent>
              <Typography variant="subtitle2" sx={{ fontWeight: 600, mb: 1.5 }}>Suggested Questions</Typography>
              {STARTERS.map(q => (
                <Box key={q}
                  onClick={() => send(q)}
                  sx={{
                    p: 1, mb: 0.75, borderRadius: 1.5, cursor: "pointer",
                    bgcolor: "rgba(255,255,255,0.03)",
                    border: "1px solid rgba(255,255,255,0.06)",
                    "&:hover": { bgcolor: "rgba(124,58,237,0.1)", borderColor: "primary.main" },
                    transition: "all 0.15s",
                  }}>
                  <Typography variant="caption" sx={{ lineHeight: 1.5 }}>{q}</Typography>
                </Box>
              ))}
            </CardContent>
          </Card>
        </Grid>
      </Grid>
    </Box>
  );
}
