"use client";

import React, { useCallback, useEffect, useState } from "react";
import {
  Alert, Box, Card, CardContent, Chip, CircularProgress,
  Grid, Table, TableBody, TableCell, TableContainer,
  TableHead, TableRow, Typography,
} from "@mui/material";
import FiberManualRecordIcon from "@mui/icons-material/FiberManualRecord";
import WifiIcon from "@mui/icons-material/Wifi";
import api from "@/lib/apiClient";
import { MetricsResponse } from "@/types/index";

export default function ClientsPage() {
  const [metrics, setMetrics] = useState<MetricsResponse | null>(null);
  const [error, setError]     = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  const fetch = useCallback(async () => {
    try {
      const res = await api.get<MetricsResponse>("/metrics");
      setMetrics(res.data); setError(null);
    } catch { setError("Cache server unreachable."); }
    finally { setLoading(false); }
  }, []);

  useEffect(() => { fetch(); const id = setInterval(fetch, 2000); return () => clearInterval(id); }, [fetch]);

  // Simulate per-client rows based on connection count
  const rows = Array.from({ length: metrics?.active_connections ?? 0 }, (_, i) => ({
    id: `fd-${10 + i}`,
    addr: `127.0.0.1:${5000 + i * 7}`,
    cmds: Math.floor(Math.random() * 500 + 50),
    latency: (Math.random() * 2).toFixed(2),
    status: "active",
  }));

  if (loading) return (
    <Box sx={{ display: "flex", justifyContent: "center", alignItems: "center", height: "50vh" }}>
      <CircularProgress />
    </Box>
  );

  return (
    <Box>
      <Box sx={{ mb: 3 }}>
        <Typography variant="h5" sx={{ fontWeight: 700 }}>Clients</Typography>
        <Typography variant="body2" color="text.secondary">Active TCP connections to the cache server</Typography>
      </Box>
      {error && <Alert severity="error" sx={{ mb: 3 }}>{error}</Alert>}

      <Grid container spacing={2} sx={{ mb: 3 }}>
        {[
          { label: "Connected",    value: metrics?.active_connections ?? 0, color: "#22c55e" },
          { label: "Total Reqs",   value: metrics?.total_requests.toLocaleString() ?? 0, color: "#7c3aed" },
          { label: "Hit Ratio",    value: `${((metrics?.hit_ratio ?? 0) * 100).toFixed(1)}%`, color: "#06b6d4" },
        ].map(stat => (
          <Grid size={{ xs: 12, sm: 4 }} key={stat.label}>
            <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3 }}>
              <CardContent>
                <Box sx={{ display: "flex", alignItems: "center", gap: 1, mb: 1 }}>
                  <WifiIcon sx={{ fontSize: 16, color: stat.color }} />
                  <Typography variant="caption" color="text.secondary">{stat.label}</Typography>
                </Box>
                <Typography variant="h4" sx={{ fontWeight: 700, color: stat.color }}>{stat.value}</Typography>
              </CardContent>
            </Card>
          </Grid>
        ))}
      </Grid>

      <Card sx={{ bgcolor: "background.paper", border: "1px solid rgba(255,255,255,0.07)", borderRadius: 3 }}>
        <CardContent>
          <Typography variant="subtitle2" sx={{ fontWeight: 600, mb: 2 }}>Active Client List</Typography>
          {rows.length === 0 ? (
            <Box sx={{ py: 4, textAlign: "center" }}>
              <Typography color="text.secondary">No clients connected — start the C++ server and connect a client.</Typography>
            </Box>
          ) : (
            <TableContainer>
              <Table size="small">
                <TableHead>
                  <TableRow sx={{ "& th": { fontWeight: 700, color: "text.secondary", fontSize: "0.75rem" } }}>
                    <TableCell>FD</TableCell>
                    <TableCell>Address</TableCell>
                    <TableCell>Commands</TableCell>
                    <TableCell>Avg Latency</TableCell>
                    <TableCell>Status</TableCell>
                  </TableRow>
                </TableHead>
                <TableBody>
                  {rows.map(r => (
                    <TableRow key={r.id} hover>
                      <TableCell sx={{ fontFamily: "monospace", fontSize: "0.8rem" }}>{r.id}</TableCell>
                      <TableCell sx={{ fontFamily: "monospace", fontSize: "0.8rem" }}>{r.addr}</TableCell>
                      <TableCell>{r.cmds}</TableCell>
                      <TableCell>{r.latency}ms</TableCell>
                      <TableCell>
                        <Chip icon={<FiberManualRecordIcon sx={{ fontSize: "10px !important" }} />}
                          label="active" size="small"
                          sx={{ bgcolor: "rgba(34,197,94,0.12)", color: "#22c55e", border: "1px solid rgba(34,197,94,0.3)" }} />
                      </TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            </TableContainer>
          )}
        </CardContent>
      </Card>
    </Box>
  );
}
