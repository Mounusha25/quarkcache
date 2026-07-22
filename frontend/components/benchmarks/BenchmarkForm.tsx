"use client";

import React, { useState } from "react";
import {
  Alert, Box, Button, Card, CardContent, CircularProgress,
  Divider, Grid, LinearProgress, Slider, TextField, Typography,
} from "@mui/material";
import PlayArrowIcon from "@mui/icons-material/PlayArrow";
import api from "@/lib/apiClient";
import { BenchmarkRequest, BenchmarkResponse } from "@/types/index";

const OPS_OPTIONS = [100, 500, 1_000, 5_000, 10_000, 50_000, 100_000];

export default function BenchmarkForm({ onResults }: { onResults: (r: BenchmarkResponse) => void }) {
  const [opsIdx, setOpsIdx]       = useState(2);
  const [readRatio, setReadRatio] = useState(0.8);
  const [valueSize, setValueSize] = useState(64);
  const [keySpace, setKeySpace]   = useState(500);
  const [loading, setLoading]     = useState(false);
  const [error, setError]         = useState<string | null>(null);

  const numOps = OPS_OPTIONS[opsIdx];

  const handleRun = async () => {
    setLoading(true); setError(null);
    try {
      const body: BenchmarkRequest = { num_ops: numOps, read_ratio: readRatio, value_size: valueSize, key_space: keySpace };
      const res = await api.post<BenchmarkResponse>("/benchmarks/run", body);
      onResults(res.data);
    } catch {
      setError("Benchmark failed — is the cache server running?");
    } finally {
      setLoading(false);
    }
  };

  return (
    <Card elevation={2}>
      <CardContent>
        <Typography variant="h6" gutterBottom sx={{ fontWeight: 600 }}>Configure Benchmark</Typography>
        <Divider sx={{ mb: 3 }} />
        <Grid container spacing={4}>
          <Grid size={{ xs: 12, sm: 6 }}>
            <Typography gutterBottom>
              Total Operations: <strong>{numOps.toLocaleString()}</strong>
            </Typography>
            <Slider value={opsIdx} min={0} max={OPS_OPTIONS.length - 1} step={1}
              onChange={(_, v) => setOpsIdx(v as number)}
              marks={OPS_OPTIONS.map((val, i) => ({
                value: i, label: val >= 1000 ? `${val / 1000}k` : String(val),
              }))}
              valueLabelDisplay="off"
            />
          </Grid>
          <Grid size={{ xs: 12, sm: 6 }}>
            <Typography gutterBottom>
              Read Ratio: <strong>{Math.round(readRatio * 100)}% GET / {Math.round((1 - readRatio) * 100)}% SET</strong>
            </Typography>
            <Slider value={readRatio} min={0} max={1} step={0.05}
              onChange={(_, v) => setReadRatio(v as number)}
              valueLabelDisplay="auto"
              valueLabelFormat={v => `${Math.round((v as number) * 100)}%`}
            />
          </Grid>
          <Grid size={{ xs: 12, sm: 6 }}>
            <TextField label="Value size (bytes)" type="number" fullWidth size="small"
              value={valueSize}
              slotProps={{ htmlInput: { min: 1, max: 4096 } }}
              onChange={e => setValueSize(Math.max(1, parseInt(e.target.value, 10) || 1))} />
          </Grid>
          <Grid size={{ xs: 12, sm: 6 }}>
            <TextField label="Key space (distinct keys)" type="number" fullWidth size="small"
              value={keySpace}
              slotProps={{ htmlInput: { min: 1 } }}
              onChange={e => setKeySpace(Math.max(1, parseInt(e.target.value, 10) || 1))} />
          </Grid>
        </Grid>
        <Box sx={{ mt: 4 }}>
          {loading && <LinearProgress sx={{ mb: 2 }} />}
          <Button variant="contained" size="large" onClick={handleRun} disabled={loading}
            startIcon={loading ? <CircularProgress size={20} color="inherit" /> : <PlayArrowIcon />}>
            {loading ? `Running ${numOps.toLocaleString()} ops…` : "Run Benchmark"}
          </Button>
          {error && <Alert severity="error" sx={{ mt: 2 }}>{error}</Alert>}
        </Box>
      </CardContent>
    </Card>
  );
}
