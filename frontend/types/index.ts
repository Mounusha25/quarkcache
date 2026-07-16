// ─── Metrics ────────────────────────────────────────────────────────────────

export interface MetricsResponse {
  total_requests: number;
  hits: number;
  misses: number;
  hit_ratio: number;
  ops_per_sec: number;
  memory_used_bytes: number;
  memory_limit_bytes: number;
  active_connections: number;
}

// ─── Keys ────────────────────────────────────────────────────────────────────

export interface KeyListItem {
  key: string;
  value: string;
  ttl_remaining: number | null;
}

export interface KeyListResponse {
  keys: KeyListItem[];
  count: number;
}

export interface SetKeyRequest {
  key: string;
  value: string;
  ttl?: number;
}

export interface KeyResponse {
  key: string;
  value: string;
  ttl_remaining: number | null;
}

export interface DeleteResponse {
  key: string;
  deleted: boolean;
}

// ─── Benchmarks ───────────────────────────────────────────────────────────────

export interface BenchmarkRequest {
  num_ops: number;
  read_ratio: number;
  value_size: number;
  key_space: number;
}

export interface LatencyPercentiles {
  p50_ms: number;
  p95_ms: number;
  p99_ms: number;
  min_ms: number;
  max_ms: number;
}

export interface BenchmarkResponse {
  num_ops: number;
  total_time_ms: number;
  ops_per_sec: number;
  latency: LatencyPercentiles;
  errors: number;
}

// ─── Health ───────────────────────────────────────────────────────────────────

export interface HealthResponse {
  status: string;
  version: string;
  cpp_server_reachable: boolean;
}
