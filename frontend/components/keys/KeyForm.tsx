"use client";

import React, { useState } from "react";
import {
  Alert, Box, Button, Card, CardContent, CircularProgress,
  Divider, Grid, TextField, Typography,
} from "@mui/material";
import AddIcon from "@mui/icons-material/Add";
import SearchIcon from "@mui/icons-material/Search";
import api from "@/lib/apiClient";
import { KeyResponse, SetKeyRequest } from "@/types/index";

export default function KeyForm({ onKeySet }: { onKeySet: () => void }) {
  const [setKey, setSetKey]     = useState("");
  const [setValue, setSetValue] = useState("");
  const [setTtl, setSetTtl]     = useState("");
  const [setLoading, setSetLoading] = useState(false);
  const [setMsg, setSetMsg] = useState<{ type: "success" | "error"; text: string } | null>(null);

  const [getKey, setGetKey]         = useState("");
  const [getLoading, setGetLoading] = useState(false);
  const [getResult, setGetResult]   = useState<KeyResponse | null>(null);
  const [getError, setGetError]     = useState<string | null>(null);

  const handleSet = async (e: React.FormEvent) => {
    e.preventDefault();
    setSetLoading(true); setSetMsg(null);
    try {
      const body: SetKeyRequest = { key: setKey, value: setValue };
      if (setTtl) body.ttl = parseInt(setTtl, 10);
      await api.post("/keys", body);
      setSetMsg({ type: "success", text: `Key "${setKey}" stored.` });
      onKeySet();
    } catch {
      setSetMsg({ type: "error", text: "Failed to set key." });
    } finally {
      setSetLoading(false);
    }
  };

  const handleGet = async (e: React.FormEvent) => {
    e.preventDefault();
    setGetLoading(true); setGetResult(null); setGetError(null);
    try {
      const res = await api.get<KeyResponse>(`/keys/${encodeURIComponent(getKey)}`);
      setGetResult(res.data);
    } catch {
      setGetError("Key not found or cache server unreachable.");
    } finally {
      setGetLoading(false);
    }
  };

  return (
    <Grid container spacing={3}>
      <Grid size={{ xs: 12, md: 6 }}>
        <Card elevation={2}>
          <CardContent>
            <Typography variant="h6" gutterBottom sx={{ fontWeight: 600 }}>Set Key</Typography>
            <Box component="form" onSubmit={handleSet} noValidate>
              <TextField label="Key" value={setKey} onChange={e => setSetKey(e.target.value)}
                fullWidth required size="small" sx={{ mb: 2 }} />
              <TextField label="Value" value={setValue} onChange={e => setSetValue(e.target.value)}
                fullWidth required size="small" sx={{ mb: 2 }} />
              <TextField label="TTL (seconds, optional)" value={setTtl} type="number"
                onChange={e => setSetTtl(e.target.value)}
                fullWidth size="small" sx={{ mb: 2 }}
                slotProps={{ htmlInput: { min: 1 } }} />
              <Button type="submit" variant="contained" fullWidth
                disabled={setLoading || !setKey || !setValue}
                startIcon={setLoading ? <CircularProgress size={16} color="inherit" /> : <AddIcon />}>
                {setLoading ? "Storing…" : "Set Key"}
              </Button>
              {setMsg && <Alert severity={setMsg.type} sx={{ mt: 2 }}>{setMsg.text}</Alert>}
            </Box>
          </CardContent>
        </Card>
      </Grid>

      <Grid size={{ xs: 12, md: 6 }}>
        <Card elevation={2}>
          <CardContent>
            <Typography variant="h6" gutterBottom sx={{ fontWeight: 600 }}>Get Key</Typography>
            <Box component="form" onSubmit={handleGet} noValidate>
              <TextField label="Key" value={getKey} onChange={e => setGetKey(e.target.value)}
                fullWidth required size="small" sx={{ mb: 2 }} />
              <Button type="submit" variant="outlined" fullWidth
                disabled={getLoading || !getKey}
                startIcon={getLoading ? <CircularProgress size={16} color="inherit" /> : <SearchIcon />}>
                {getLoading ? "Looking up…" : "Get Key"}
              </Button>
              {getResult && (
                <Box sx={{ mt: 2, p: 2, bgcolor: "action.hover", borderRadius: 1 }}>
                  <Typography variant="caption" color="text.secondary">Value</Typography>
                  <Typography sx={{ fontFamily: "monospace", wordBreak: "break-all" }}>{getResult.value}</Typography>
                  {getResult.ttl_remaining !== null && (
                    <>
                      <Divider sx={{ my: 1 }} />
                      <Typography variant="caption" color="text.secondary">
                        TTL remaining: {getResult.ttl_remaining}s
                      </Typography>
                    </>
                  )}
                </Box>
              )}
              {getError && <Alert severity="error" sx={{ mt: 2 }}>{getError}</Alert>}
            </Box>
          </CardContent>
        </Card>
      </Grid>
    </Grid>
  );
}
