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
  aof_enabled: boolean;
  aof_size_bytes: number;
  aof_writes: number;
  aof_fsyncs: number;
  aof_recovered_records: number;
  aof_truncated_bytes: number;
  aof_compactions: number;
  aof_reclaimed_bytes: number;
}

export interface CompactionResponse {
  before_bytes: number;
  after_bytes: number;
  reclaimed_bytes: number;
  records: number;
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
