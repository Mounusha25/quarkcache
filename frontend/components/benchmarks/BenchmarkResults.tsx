"use client";

import React from "react";
import { Box, Card, CardContent, Chip, Divider, Grid, Typography } from "@mui/material";
import { BenchmarkResponse } from "@/types/index";

function StatBox({ label, value, unit }: { label: string; value: string | number; unit?: string }) {
  return (
    <Box sx={{ textAlign: "center", p: 2 }}>
      <Typography variant="h4" sx={{ fontWeight: 700, color: "primary.main" }}>
        {value}
        {unit && (
          <Typography component="span" variant="body1" color="text.secondary" sx={{ ml: 0.5 }}>
            {unit}
          </Typography>
        )}
      </Typography>
      <Typography variant="caption" color="text.secondary">{label}</Typography>
    </Box>
  );
}

export default function BenchmarkResults({ result }: { result: BenchmarkResponse }) {
  const { latency, errors } = result;
  return (
    <Card elevation={2}>
      <CardContent>
        <Box sx={{ display: "flex", alignItems: "center", gap: 2, mb: 2 }}>
          <Typography variant="h6" sx={{ fontWeight: 600 }}>Results</Typography>
          {errors > 0 && <Chip label={`${errors} errors`} color="warning" size="small" />}
        </Box>
        <Divider sx={{ mb: 2 }} />
        <Grid container>
          <Grid size={{ xs: 6, sm: 3 }}>
            <StatBox label="Operations" value={result.num_ops.toLocaleString()} />
          </Grid>
          <Grid size={{ xs: 6, sm: 3 }}>
            <StatBox label="Total time" value={result.total_time_ms.toFixed(0)} unit="ms" />
          </Grid>
          <Grid size={{ xs: 6, sm: 3 }}>
            <StatBox label="Throughput"
              value={result.ops_per_sec.toLocaleString(undefined, { maximumFractionDigits: 0 })}
              unit="ops/s" />
          </Grid>
          <Grid size={{ xs: 6, sm: 3 }}>
            <StatBox label="Errors" value={errors} />
          </Grid>
        </Grid>
        <Divider sx={{ my: 2 }} />
        <Typography variant="subtitle2" sx={{ fontWeight: 600, mb: 1 }}>Latency percentiles</Typography>
        <Grid container>
          {[
            { label: "Min", val: latency.min_ms },
            { label: "p50", val: latency.p50_ms },
            { label: "p95", val: latency.p95_ms },
            { label: "p99", val: latency.p99_ms },
            { label: "Max", val: latency.max_ms },
          ].map(({ label, val }) => (
            <Grid size="grow" key={label}>
              <Box sx={{ textAlign: "center", py: 1 }}>
                <Typography variant="h6" sx={{ fontWeight: 600 }}>
                  {val.toFixed(2)}
                  <Typography component="span" variant="caption" color="text.secondary" sx={{ ml: 0.5 }}>ms</Typography>
                </Typography>
                <Typography variant="caption" color="text.secondary">{label}</Typography>
              </Box>
            </Grid>
          ))}
        </Grid>
      </CardContent>
    </Card>
  );
}
