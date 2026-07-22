"use client";

import React, { useState } from "react";
import { Box, Typography } from "@mui/material";
import BenchmarkForm from "@/components/benchmarks/BenchmarkForm";
import BenchmarkResults from "@/components/benchmarks/BenchmarkResults";
import { BenchmarkResponse } from "@/types/index";

export default function BenchmarksPage() {
  const [result, setResult] = useState<BenchmarkResponse | null>(null);

  return (
    <Box>
      <Box sx={{ mb: 3 }}>
        <Typography variant="h5" sx={{ fontWeight: 700 }}>Benchmarks</Typography>
        <Typography variant="body2" color="text.secondary">
          Run a workload against the cache and inspect throughput and latency stats
        </Typography>
      </Box>
      <BenchmarkForm onResults={setResult} />
      {result && <Box sx={{ mt: 4 }}><BenchmarkResults result={result} /></Box>}
    </Box>
  );
}
