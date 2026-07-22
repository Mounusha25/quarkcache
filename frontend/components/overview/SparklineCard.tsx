"use client";

import React, { useEffect, useState } from "react";
import { Box, Card, CardContent, Typography } from "@mui/material";
import { ResponsiveContainer, AreaChart, Area } from "recharts";

interface SparklineCardProps {
  title: string;
  value: string;
  delta?: string;
  positive?: boolean;
  data: { v: number }[];
  color: string;
}

export default function SparklineCard({ title, value, delta, positive = true, data, color }: SparklineCardProps) {
  const [mounted, setMounted] = useState(false);
  useEffect(() => setMounted(true), []);

  return (
    <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3 }}>
      <CardContent sx={{ pb: "12px !important" }}>
        <Typography variant="caption" color="text.secondary"
          sx={{ letterSpacing: "0.05em", textTransform: "uppercase", fontSize: "0.7rem" }}>
          {title}
        </Typography>
        <Typography variant="h4" sx={{ fontWeight: 700, mt: 0.5, fontSize: "1.8rem" }}>{value}</Typography>
        {delta && (
          <Typography variant="caption" sx={{ color: positive ? "#22c55e" : "#ef4444", fontWeight: 600 }}>
            {positive ? "+" : ""}{delta}
          </Typography>
        )}
        <Box sx={{ mt: 1, height: 40, minWidth: 0 }}>
          {mounted && (
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={data} margin={{ top: 0, right: 0, bottom: 0, left: 0 }}>
                <defs>
                  <linearGradient id={`grad-${title.replace(/\s/g, "")}`} x1="0" y1="0" x2="0" y2="1">
                    <stop offset="5%" stopColor={color} stopOpacity={0.3} />
                    <stop offset="95%" stopColor={color} stopOpacity={0} />
                  </linearGradient>
                </defs>
                <Area type="monotone" dataKey="v" stroke={color} strokeWidth={2}
                  fill={`url(#grad-${title.replace(/\s/g, "")})`} dot={false} isAnimationActive={false} />
              </AreaChart>
            </ResponsiveContainer>
          )}
        </Box>
      </CardContent>
    </Card>
  );
}
