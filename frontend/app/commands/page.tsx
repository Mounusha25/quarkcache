"use client";

import React, { useRef, useState } from "react";
import {
  Alert, Box, Card, CardContent, Chip, CircularProgress,
  Divider, IconButton, InputAdornment, TextField, Typography,
} from "@mui/material";
import SendIcon from "@mui/icons-material/Send";
import api from "@/lib/apiClient";
import { KeyResponse } from "@/types/index";

interface LogEntry {
  id: number;
  cmd: string;
  response: string;
  ok: boolean;
  ms: number;
  ts: string;
}

let seq = 0;

export default function CommandsPage() {
  const [input, setInput]     = useState("");
  const [log, setLog]         = useState<LogEntry[]>([]);
  const [loading, setLoading] = useState(false);
  const [hint, setHint]       = useState<string | null>(null);
  const bottomRef = useRef<HTMLDivElement>(null);

  const addLog = (entry: LogEntry) => {
    setLog(prev => [...prev.slice(-199), entry]);
    setTimeout(() => bottomRef.current?.scrollIntoView({ behavior: "smooth" }), 50);
  };

  const run = async () => {
    const raw = input.trim();
    if (!raw) return;
    setInput("");
    setLoading(true);
    setHint(null);
    const t0 = performance.now();

    const parts = raw.split(/\s+/);
    const cmd = parts[0].toUpperCase();
    let response = "";
    let ok = true;

    try {
      if (cmd === "SET") {
        const [, key, value, ttl] = parts;
        if (!key || !value) { response = "Usage: SET <key> <value> [ttl]"; ok = false; }
        else {
          await api.post("/keys", { key, value, ttl: ttl ? parseInt(ttl) : undefined });
          response = "OK";
        }
      } else if (cmd === "GET") {
        const res = await api.get<KeyResponse>(`/keys/${encodeURIComponent(parts[1] ?? "")}`);
        response = `"${res.data.value}"${res.data.ttl_remaining !== null ? ` (TTL: ${res.data.ttl_remaining}s)` : ""}`;
      } else if (cmd === "DEL") {
        const res = await api.delete<{ deleted: boolean }>(`/keys/${encodeURIComponent(parts[1] ?? "")}`);
        response = res.data.deleted ? "1" : "0";
      } else if (cmd === "KEYS") {
        const res = await api.get<{ keys: { key: string }[] }>("/keys");
        response = res.data.keys.map(k => `"${k.key}"`).join("\n") || "(empty)";
      } else if (cmd === "STATS") {
        const res = await api.get("/metrics");
        response = JSON.stringify(res.data, null, 2);
      } else if (cmd === "HELP") {
        response = "Commands: SET <key> <value> [ttl]  |  GET <key>  |  DEL <key>  |  KEYS  |  STATS  |  CLEAR";
      } else if (cmd === "CLEAR") {
        setLog([]); setLoading(false); return;
      } else {
        response = `ERR unknown command '${parts[0]}'`; ok = false;
        setHint("Try: SET mykey hello 60  |  GET mykey  |  DEL mykey  |  KEYS  |  STATS");
      }
    } catch (e: unknown) {
      const err = e as { response?: { data?: { detail?: string } } };
      response = `ERR ${err?.response?.data?.detail ?? "server error"}`;
      ok = false;
    }

    addLog({ id: ++seq, cmd: raw, response, ok, ms: Math.round(performance.now() - t0), ts: new Date().toLocaleTimeString() });
    setLoading(false);
  };

  return (
    <Box>
      <Box sx={{ mb: 3 }}>
        <Typography variant="h5" sx={{ fontWeight: 700 }}>Commands</Typography>
        <Typography variant="body2" color="text.secondary">Interactive REPL — type commands like a Redis CLI</Typography>
      </Box>

      <Card sx={{ bgcolor: "#0d0d12", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3, mb: 2 }}>
        <CardContent sx={{ pb: "12px !important" }}>
          {/* Log area */}
          <Box sx={{ minHeight: 340, maxHeight: 400, overflowY: "auto", fontFamily: "monospace", fontSize: "0.82rem", mb: 2 }}>
            {log.length === 0 && (
              <Typography color="text.disabled" sx={{ fontFamily: "monospace", fontSize: "0.82rem" }}>
                {`> Type a command below. Try: SET name QuarkCache 60`}
              </Typography>
            )}
            {log.map(entry => (
              <Box key={entry.id} sx={{ mb: 1.5 }}>
                <Box sx={{ display: "flex", alignItems: "center", gap: 1 }}>
                  <Typography component="span" sx={{ color: "#7c3aed", fontFamily: "inherit", fontSize: "inherit" }}>❯</Typography>
                  <Typography component="span" sx={{ color: "#e2e8f0", fontFamily: "inherit", fontSize: "inherit" }}>{entry.cmd}</Typography>
                  <Typography component="span" sx={{ color: "#4b5563", fontFamily: "inherit", fontSize: "0.72rem", ml: "auto" }}>{entry.ts} · {entry.ms}ms</Typography>
                </Box>
                <Typography sx={{
                  color: entry.ok ? "#22c55e" : "#ef4444",
                  fontFamily: "monospace", fontSize: "0.82rem",
                  whiteSpace: "pre-wrap", ml: 2,
                }}>
                  {entry.response}
                </Typography>
              </Box>
            ))}
            <div ref={bottomRef} />
          </Box>
          <Divider sx={{ borderColor: "rgba(255,255,255,0.06)", mb: 1.5 }} />
          {hint && <Alert severity="info" sx={{ mb: 1.5, py: 0.5, fontSize: "0.78rem" }}>{hint}</Alert>}
          <TextField
            value={input}
            onChange={e => setInput(e.target.value)}
            onKeyDown={e => e.key === "Enter" && !loading && run()}
            placeholder="SET key value [ttl]  •  GET key  •  DEL key  •  KEYS  •  STATS  •  HELP"
            fullWidth size="small" autoFocus
            sx={{ "& .MuiOutlinedInput-root": { fontFamily: "monospace", bgcolor: "rgba(255,255,255,0.03)" } }}
            slotProps={{
              input: {
                endAdornment: (
                  <InputAdornment position="end">
                    {loading
                      ? <CircularProgress size={18} />
                      : <IconButton size="small" onClick={run} disabled={!input.trim()}><SendIcon fontSize="small" /></IconButton>
                    }
                  </InputAdornment>
                ),
              },
            }}
          />
        </CardContent>
      </Card>

      {/* Quick reference */}
      <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3 }}>
        <CardContent>
          <Typography variant="subtitle2" sx={{ fontWeight: 600, mb: 1.5 }}>Quick Reference</Typography>
          <Box sx={{ display: "flex", flexWrap: "wrap", gap: 1 }}>
            {["SET key value [ttl]", "GET key", "DEL key", "KEYS", "STATS", "HELP", "CLEAR"].map(c => (
              <Chip key={c} label={c} size="small" variant="outlined"
                onClick={() => setInput(c.split(" ")[0] + " ")}
                sx={{ fontFamily: "monospace", cursor: "pointer", fontSize: "0.75rem" }} />
            ))}
          </Box>
        </CardContent>
      </Card>
    </Box>
  );
}
