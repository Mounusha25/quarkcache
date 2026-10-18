#pragma once

#include "quarkcache/aof.hpp"

#include <string>
#include <optional>
#include <chrono>
#include <mutex>
#include <atomic>
#include <memory>

namespace quarkcache {

class LruCache;

struct Stats {
    uint64_t total_requests{0};
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t bytes_used{0};
    uint32_t active_connections{0};
    bool aof_enabled{false};
    uint64_t aof_size_bytes{0};
    uint64_t aof_writes{0};
    uint64_t aof_fsyncs{0};
    uint64_t aof_recovered_records{0};
    uint64_t aof_truncated_bytes{0};
    uint64_t aof_compactions{0};
    uint64_t aof_reclaimed_bytes{0};
};

/**
 * KVStore is the public-facing thread-safe cache facade.
 *
 * It wraps an LruCache and exposes simple set/get/del operations
 * while tracking hit/miss metrics and connection counts.
 */
class KVStore {
public:
    explicit KVStore(
        size_t max_keys = 100'000,
        std::shared_ptr<AppendOnlyFile> persistence = nullptr
    );
    ~KVStore();

    bool set(const std::string& key,
             const std::string& value,
             std::optional<std::chrono::seconds> ttl = std::nullopt);

    std::optional<std::string> get(const std::string& key);

    bool del(const std::string& key);

    // Restore mutations without recording them again or affecting request metrics.
    void restore();

    // Atomically replace the AOF with one SET record per currently live key.
    AofCompactionResult compact_persistence();

    // Prune TTL-expired keys; call from a background thread periodically.
    void evict_expired();

    Stats get_stats() const;

    void increment_connections();
    void decrement_connections();

    // Direct LruCache access for the key-listing endpoint (hold the lock yourself).
    LruCache* lru();
    std::mutex& mutex() { return mu_; }

private:
    std::unique_ptr<LruCache> lru_;
    std::shared_ptr<AppendOnlyFile> persistence_;
    mutable std::mutex mu_;

    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> hits_{0};
    std::atomic<uint64_t> misses_{0};
    std::atomic<uint32_t> active_connections_{0};
};

} // namespace quarkcache
