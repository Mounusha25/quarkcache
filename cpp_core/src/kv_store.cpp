#include "quarkcache/kv_store.hpp"
#include "quarkcache/aof.hpp"
#include "quarkcache/lru_cache.hpp"

namespace quarkcache {

KVStore::KVStore(size_t max_keys, std::shared_ptr<AppendOnlyFile> persistence)
    : lru_(std::make_unique<LruCache>(max_keys)), persistence_(std::move(persistence)) {}

// Need a definition here because LruCache is forward-declared in the header.
KVStore::~KVStore() = default;

bool KVStore::set(const std::string& key,
                  const std::string& value,
                  std::optional<std::chrono::seconds> ttl) {
    std::lock_guard<std::mutex> lock(mu_);
    ++total_requests_;
    if (persistence_) {
        const auto expires_at = ttl
            ? std::make_optional(std::chrono::system_clock::now() + *ttl)
            : std::nullopt;
        persistence_->append_set(key, value, expires_at);
    }
    return lru_->set(key, value, ttl);
}

std::optional<std::string> KVStore::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mu_);
    ++total_requests_;
    auto result = lru_->get(key);
    if (result) {
        ++hits_;
    } else {
        ++misses_;
    }
    return result;
}

bool KVStore::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mu_);
    ++total_requests_;
    if (persistence_) persistence_->append_delete(key);
    return lru_->del(key);
}

void KVStore::restore() {
    if (!persistence_) return;
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& record : persistence_->recovered_records()) {
        if (record.operation == AofOperation::Delete) {
            lru_->del(record.key);
            continue;
        }
        std::optional<std::chrono::seconds> ttl;
        if (record.expires_at) {
            const auto remaining = *record.expires_at - std::chrono::system_clock::now();
            if (remaining <= std::chrono::system_clock::duration::zero()) {
                lru_->del(record.key);
                continue;
            }
            ttl = std::chrono::duration_cast<std::chrono::seconds>(remaining);
            if (*ttl <= std::chrono::seconds::zero()) ttl = std::chrono::seconds(1);
        }
        lru_->set(record.key, record.value, ttl);
    }
}

void KVStore::evict_expired() {
    std::lock_guard<std::mutex> lock(mu_);
    lru_->evict_expired();
}

Stats KVStore::get_stats() const {
    std::lock_guard<std::mutex> lock(mu_);
    Stats s;
    s.total_requests = total_requests_.load();
    s.hits = hits_.load();
    s.misses = misses_.load();
    s.bytes_used = lru_->bytes_used();
    s.active_connections = active_connections_.load();
    if (persistence_) {
        const auto aof = persistence_->stats();
        s.aof_enabled = true;
        s.aof_size_bytes = aof.size_bytes;
        s.aof_writes = aof.writes;
        s.aof_fsyncs = aof.fsyncs;
        s.aof_recovered_records = aof.recovered_records;
        s.aof_truncated_bytes = aof.truncated_bytes;
    }
    return s;
}

void KVStore::increment_connections() { ++active_connections_; }
void KVStore::decrement_connections() {
    if (active_connections_ > 0) --active_connections_;
}

LruCache* KVStore::lru() { return lru_.get(); }

} // namespace quarkcache
