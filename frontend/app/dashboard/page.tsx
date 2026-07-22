"use client";

import React, { useCallback, useEffect, useRef, useState } from "react";
import { Alert, Box, CircularProgress, Grid, Typography } from "@mui/material";
import SparklineCard from "@/components/overview/SparklineCard";
import OpsChart from "@/components/overview/OpsChart";
import KeyspaceChart from "@/components/overview/KeyspaceChart";
import api from "@/lib/apiClient";
import { MetricsResponse } from "@/types/index";

const REFRESH_MS = 2000;
const MAX_HISTORY = 30;

function fmtBytes(b: number): string {
  if (b < 1024) return `${b} B`;
  if (b < 1024 ** 2) return `${(b / 1024).toFixed(1)} KB`;
  return `${(b / 1024 ** 2).toFixed(1)} MB`;
}

function nowLabel() {
  return new Date().toLocaleTimeString("en-US", { hour: "2-digit", minute: "2-digit", second: "2-digit", hour12: false });
}

export default function DashboardPage() {
  const [metrics, setMetrics] = useState<MetricsResponse | null>(null);
  const [error, setError]     = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  // Rolling history for sparklines and OPS chart
  const opsHistory    = useRef<{ v: number }[]>([]);
  const hitsHistory   = useRef<{ v: number }[]>([]);
  const memHistory    = useRef<{ v: number }[]>([]);
  const connsHistory  = useRef<{ v: number }[]>([]);
  const opsChart      = useRef<{ time: string; ops: number }[]>([]);
  const [, forceRender] = useState(0);

  const fetchMetrics = useCallback(async () => {
    try {
      const res = await api.get<MetricsResponse>("/metrics");
      const m = res.data;
      setMetrics(m);
      setError(null);

      const push = <T,>(arr: React.MutableRefObject<T[]>, val: T) => {
        arr.current.push(val);
        if (arr.current.length > MAX_HISTORY) arr.current.shift();
      };
      push(opsHistory,   { v: m.ops_per_sec });
      push(hitsHistory,  { v: Math.round(m.hit_ratio * 100) });
      push(memHistory,   { v: m.memory_used_bytes });
      push(connsHistory, { v: m.active_connections });
      push(opsChart,     { time: nowLabel(), ops: Math.round(m.ops_per_sec) });
    } catch {
      setError("Cache server unreachable. Start the C++ server on port 9000.");
    } finally {
      setLoading(false);
      forceRender(n => n + 1);
    }
  }, []);

  useEffect(() => {
    fetchMetrics();
    const id = setInterval(fetchMetrics, REFRESH_MS);
    return () => clearInterval(id);
  }, [fetchMetrics]);

  if (loading) {
    return (
      <Box sx={{ display: "flex", justifyContent: "center", alignItems: "center", height: "50vh" }}>
        <CircularProgress />
      </Box>
    );
  }

  return (
    <Box>
      <Box sx={{ mb: 3 }}>
        <Typography variant="h5" sx={{ fontWeight: 700 }}>Overview</Typography>
        <Typography variant="body2" color="text.secondary">
          Real-time cache metrics — refreshes every {REFRESH_MS / 1000}s
        </Typography>
      </Box>

      {error && <Alert severity="error" sx={{ mb: 3 }}>{error}</Alert>}

      {metrics && (
        <>
          {/* Stat cards */}
          <Grid container spacing={2} sx={{ mb: 2 }}>
            <Grid size={{ xs: 12, sm: 6, lg: 3 }}>
              <SparklineCard title="Total Keys" value={metrics.total_requests.toLocaleString()}
                delta="12.5%" positive data={opsHistory.current} color="#06b6d4" />
            </Grid>
            <Grid size={{ xs: 12, sm: 6, lg: 3 }}>
              <SparklineCard title="Memory Usage" value={fmtBytes(metrics.memory_used_bytes)}
                delta="8.3%" positive data={memHistory.current.map(d => ({ v: d.v / 1024 }))} color="#22c55e" />
            </Grid>
            <Grid size={{ xs: 12, sm: 6, lg: 3 }}>
              <SparklineCard title="Hits" value={`${(metrics.hit_ratio * 100).toFixed(2)}%`}
                delta="2.1%" positive data={hitsHistory.current} color="#a78bfa" />
            </Grid>
            <Grid size={{ xs: 12, sm: 6, lg: 3 }}>
              <SparklineCard title="QPS" value={Math.round(metrics.ops_per_sec).toLocaleString()}
                delta="15.7%" positive data={opsHistory.current} color="#f59e0b" />
            </Grid>
          </Grid>

          {/* Charts */}
          <Grid container spacing={2}>
            <Grid size={{ xs: 12, md: 5 }}>
              <KeyspaceChart />
            </Grid>
            <Grid size={{ xs: 12, md: 7 }}>
              <OpsChart data={opsChart.current} />
            </Grid>
          </Grid>
        </>
      )}
    </Box>
  );
}
