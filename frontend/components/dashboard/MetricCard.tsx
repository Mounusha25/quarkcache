"use client";

import React from "react";
import { Card, CardContent, Typography, Box, SxProps, Theme } from "@mui/material";

interface MetricCardProps {
  title: string;
  value: string | number;
  subtitle?: string;
  icon: React.ReactNode;
  color?: string;
  sx?: SxProps<Theme>;
}

export default function MetricCard({ title, value, subtitle, icon, color = "primary.main", sx }: MetricCardProps) {
  return (
    <Card elevation={2} sx={{ height: "100%", ...sx }}>
      <CardContent>
        <Box sx={{ display: "flex", alignItems: "center", justifyContent: "space-between", mb: 1 }}>
          <Typography variant="body2" color="text.secondary" sx={{ fontWeight: 500 }}>
            {title}
          </Typography>
          <Box sx={{ bgcolor: `${color}20`, color, borderRadius: "50%", p: 0.75, display: "flex" }}>
            {icon}
          </Box>
        </Box>
        <Typography variant="h4" sx={{ fontWeight: 700, color }}>{value}</Typography>
        {subtitle && <Typography variant="caption" color="text.secondary">{subtitle}</Typography>}
      </CardContent>
    </Card>
  );
}
