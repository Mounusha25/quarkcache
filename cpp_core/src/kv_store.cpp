#include "quarkcache/kv_store.hpp"
#include "quarkcache/lru_cache.hpp"

namespace quarkcache {

KVStore::KVStore(size_t max_keys)
    : lru_(std::make_unique<LruCache>(max_keys)) {}

// Need a definition here because LruCache is forward-declared in the header.
KVStore::~KVStore() = default;

bool KVStore::set(const std::string& key,
                  const std::string& value,
                  std::optional<std::chrono::seconds> ttl) {
    std::lock_guard<std::mutex> lock(mu_);
    ++total_requests_;
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
    return lru_->del(key);
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
    return s;
}

void KVStore::increment_connections() { ++active_connections_; }
void KVStore::decrement_connections() {
    if (active_connections_ > 0) --active_connections_;
}

LruCache* KVStore::lru() { return lru_.get(); }

} // namespace quarkcache
