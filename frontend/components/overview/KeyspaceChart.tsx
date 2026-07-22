"use client";

import React, { useEffect, useState } from "react";
import { Box, Card, CardContent, Typography } from "@mui/material";
import { ResponsiveContainer, PieChart, Pie, Cell, Tooltip } from "recharts";

const KEYSPACE_DATA = [
  { name: "Strings", value: 65, color: "#06b6d4" },
  { name: "Hashes",  value: 20, color: "#22c55e" },
  { name: "Lists",   value: 10, color: "#3b82f6" },
  { name: "Sets",    value: 5,  color: "#f59e0b" },
  { name: "ZSets",   value: 0,  color: "#8b5cf6" },
];

export default function KeyspaceChart() {
  const [mounted, setMounted] = useState(false);
  useEffect(() => setMounted(true), []);

  return (
    <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3, height: "100%" }}>
      <CardContent>
        <Typography variant="subtitle2" sx={{ fontWeight: 600, mb: 2 }}>Keyspace</Typography>
        <Box sx={{ display: "flex", alignItems: "center", gap: 3 }}>
          {/* Donut */}
          <Box sx={{ width: 130, height: 130, flexShrink: 0, minWidth: 0 }}>
            {mounted && (
              <ResponsiveContainer width="100%" height="100%">
                <PieChart>
                  <Pie data={KEYSPACE_DATA} cx="50%" cy="50%"
                    innerRadius={38} outerRadius={58}
                    dataKey="value" startAngle={90} endAngle={-270}
                    strokeWidth={0}>
                    {KEYSPACE_DATA.map((entry, i) => (
                      <Cell key={i} fill={entry.color} />
                    ))}
                  </Pie>
                  <Tooltip
                    contentStyle={{ background: "#1a1a24", border: "1px solid rgba(255,255,255,0.1)", borderRadius: 8, fontSize: 12 }}
                    formatter={(v) => [`${v}%`, ""]}
                  />
                </PieChart>
              </ResponsiveContainer>
            )}
          </Box>

          {/* Legend */}
          <Box sx={{ flexGrow: 1 }}>
            {KEYSPACE_DATA.map((item) => (
              <Box key={item.name} sx={{ display: "flex", alignItems: "center", justifyContent: "space-between", mb: 0.8 }}>
                <Box sx={{ display: "flex", alignItems: "center", gap: 1 }}>
                  <Box sx={{ width: 8, height: 8, borderRadius: "50%", bgcolor: item.color }} />
                  <Typography variant="caption" color="text.secondary">{item.name}</Typography>
                </Box>
                <Typography variant="caption" sx={{ fontWeight: 600 }}>{item.value}%</Typography>
              </Box>
            ))}
          </Box>
        </Box>
      </CardContent>
    </Card>
  );
}
