"use client";

import React, { useEffect, useState } from "react";
import { Box, Card, CardContent, Typography } from "@mui/material";
import {
  ResponsiveContainer, AreaChart, Area,
  XAxis, YAxis, CartesianGrid, Tooltip,
} from "recharts";

interface OpsDataPoint { time: string; ops: number }

export default function OpsChart({ data }: { data: OpsDataPoint[] }) {
  const [mounted, setMounted] = useState(false);
  useEffect(() => setMounted(true), []);

  return (
    <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3, height: "100%" }}>
      <CardContent>
        <Typography variant="subtitle2" sx={{ fontWeight: 600, mb: 2 }}>
          Operations{" "}
          <Typography component="span" variant="caption" color="text.secondary">(per second)</Typography>
        </Typography>
        <Box sx={{ height: 180, minWidth: 0 }}>
          {mounted && (
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={data} margin={{ top: 5, right: 5, bottom: 0, left: -20 }}>
                <defs>
                  <linearGradient id="opsGrad" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="5%" stopColor="#7c3aed" stopOpacity={0.4} />
                    <stop offset="95%" stopColor="#7c3aed" stopOpacity={0} />
                  </linearGradient>
                </defs>
                <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.05)" />
                <XAxis dataKey="time" tick={{ fontSize: 10, fill: "#6b7280" }} tickLine={false} axisLine={false} />
                <YAxis tick={{ fontSize: 10, fill: "#6b7280" }} tickLine={false} axisLine={false}
                  tickFormatter={v => v >= 1000 ? `${v / 1000}k` : String(v)} />
                <Tooltip
                  contentStyle={{ background: "#1a1a24", border: "1px solid rgba(255,255,255,0.1)", borderRadius: 8, fontSize: 12 }}
                  labelStyle={{ color: "#9ca3af" }}
                  itemStyle={{ color: "#7c3aed" }}
                />
                <Area type="monotone" dataKey="ops" stroke="#7c3aed" strokeWidth={2}
                  fill="url(#opsGrad)" dot={false} isAnimationActive={false} name="ops/sec" />
              </AreaChart>
            </ResponsiveContainer>
          )}
        </Box>
      </CardContent>
    </Card>
  );
}
