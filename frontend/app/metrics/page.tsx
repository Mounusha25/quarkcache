"use client";

import React, { useCallback, useEffect, useRef, useState } from "react";
import { Alert, Box, Card, CardContent, CircularProgress, Grid, Typography } from "@mui/material";
import {
  ResponsiveContainer, LineChart, Line, AreaChart, Area,
  XAxis, YAxis, CartesianGrid, Tooltip, Legend,
} from "recharts";
import api from "@/lib/apiClient";
import { MetricsResponse } from "@/types/index";

const MAX = 60;
const REFRESH_MS = 1500;

interface Point { time: string; ops: number; hits: number; misses: number; mem: number; conns: number }

function nowLabel() {
  return new Date().toLocaleTimeString("en-US", { hour: "2-digit", minute: "2-digit", second: "2-digit", hour12: false });
}

function ChartCard({ title, children }: { title: string; children: React.ReactNode }) {
  const [mounted, setMounted] = useState(false);
  useEffect(() => setMounted(true), []);
  return (
    <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3 }}>
      <CardContent>
        <Typography variant="subtitle2" sx={{ fontWeight: 600, mb: 2 }}>{title}</Typography>
        <Box sx={{ height: 200, minWidth: 0 }}>{mounted ? children : null}</Box>
      </CardContent>
    </Card>
  );
}

export default function MetricsPage() {
  const history = useRef<Point[]>([]);
  const [, rerender] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  const fetch = useCallback(async () => {
    try {
      const res = await api.get<MetricsResponse>("/metrics");
      const m = res.data;
      history.current.push({
        time: nowLabel(),
        ops: Math.round(m.ops_per_sec),
        hits: m.hits,
        misses: m.misses,
        mem: Math.round(m.memory_used_bytes / 1024),
        conns: m.active_connections,
      });
      if (history.current.length > MAX) history.current.shift();
      setError(null);
    } catch {
      setError("Cache server unreachable.");
    } finally {
      setLoading(false);
      rerender(n => n + 1);
    }
  }, []);

  useEffect(() => {
    fetch();
    const id = setInterval(fetch, REFRESH_MS);
    return () => clearInterval(id);
  }, [fetch]);

  const data = history.current;

  const chartProps = {
    margin: { top: 5, right: 5, bottom: 0, left: -20 },
  };

  const axisProps = {
    tick: { fontSize: 10, fill: "#6b7280" },
    tickLine: false as const,
    axisLine: false as const,
  };

  const gridProps = { strokeDasharray: "3 3", stroke: "rgba(255,255,255,0.05)" };

  const tooltipStyle = {
    contentStyle: { background: "#1a1a24", border: "1px solid rgba(255,255,255,0.1)", borderRadius: 8, fontSize: 11 },
  };

  if (loading) return (
    <Box sx={{ display: "flex", justifyContent: "center", alignItems: "center", height: "50vh" }}>
      <CircularProgress />
    </Box>
  );

  return (
    <Box>
      <Box sx={{ mb: 3 }}>
        <Typography variant="h5" sx={{ fontWeight: 700 }}>Metrics</Typography>
        <Typography variant="body2" color="text.secondary">Time-series view — last {MAX} samples</Typography>
      </Box>
      {error && <Alert severity="error" sx={{ mb: 3 }}>{error}</Alert>}
      <Grid container spacing={2}>

        {/* OPS/sec */}
        <Grid size={{ xs: 12, md: 6 }}>
          <ChartCard title="Throughput — ops/sec">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={data} {...chartProps}>
                <defs>
                  <linearGradient id="g1" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="5%" stopColor="#7c3aed" stopOpacity={0.3} />
                    <stop offset="95%" stopColor="#7c3aed" stopOpacity={0} />
                  </linearGradient>
                </defs>
                <CartesianGrid {...gridProps} />
                <XAxis dataKey="time" {...axisProps} />
                <YAxis {...axisProps} />
                <Tooltip {...tooltipStyle} />
                <Area type="monotone" dataKey="ops" stroke="#7c3aed" fill="url(#g1)"
                  strokeWidth={2} dot={false} isAnimationActive={false} name="ops/sec" />
              </AreaChart>
            </ResponsiveContainer>
          </ChartCard>
        </Grid>

        {/* Hits vs Misses */}
        <Grid size={{ xs: 12, md: 6 }}>
          <ChartCard title="Hits vs Misses (cumulative)">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={data} {...chartProps}>
                <CartesianGrid {...gridProps} />
                <XAxis dataKey="time" {...axisProps} />
                <YAxis {...axisProps} />
                <Tooltip {...tooltipStyle} />
                <Legend wrapperStyle={{ fontSize: 11 }} />
                <Line type="monotone" dataKey="hits" stroke="#22c55e" strokeWidth={2}
                  dot={false} isAnimationActive={false} name="Hits" />
                <Line type="monotone" dataKey="misses" stroke="#ef4444" strokeWidth={2}
                  dot={false} isAnimationActive={false} name="Misses" />
              </LineChart>
            </ResponsiveContainer>
          </ChartCard>
        </Grid>

        {/* Memory */}
        <Grid size={{ xs: 12, md: 6 }}>
          <ChartCard title="Memory Used (KB)">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={data} {...chartProps}>
                <defs>
                  <linearGradient id="g2" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="5%" stopColor="#f59e0b" stopOpacity={0.3} />
                    <stop offset="95%" stopColor="#f59e0b" stopOpacity={0} />
                  </linearGradient>
                </defs>
                <CartesianGrid {...gridProps} />
                <XAxis dataKey="time" {...axisProps} />
                <YAxis {...axisProps} />
                <Tooltip {...tooltipStyle} />
                <Area type="monotone" dataKey="mem" stroke="#f59e0b" fill="url(#g2)"
                  strokeWidth={2} dot={false} isAnimationActive={false} name="KB" />
              </AreaChart>
            </ResponsiveContainer>
          </ChartCard>
        </Grid>

        {/* Connections */}
        <Grid size={{ xs: 12, md: 6 }}>
          <ChartCard title="Active Connections">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={data} {...chartProps}>
                <defs>
                  <linearGradient id="g3" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="5%" stopColor="#06b6d4" stopOpacity={0.3} />
                    <stop offset="95%" stopColor="#06b6d4" stopOpacity={0} />
                  </linearGradient>
                </defs>
                <CartesianGrid {...gridProps} />
                <XAxis dataKey="time" {...axisProps} />
                <YAxis {...axisProps} allowDecimals={false} />
                <Tooltip {...tooltipStyle} />
                <Area type="monotone" dataKey="conns" stroke="#06b6d4" fill="url(#g3)"
                  strokeWidth={2} dot={false} isAnimationActive={false} name="connections" />
              </AreaChart>
            </ResponsiveContainer>
          </ChartCard>
        </Grid>

      </Grid>
    </Box>
  );
}
