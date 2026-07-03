#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <list>
#include <vector>
#include <unordered_map>

namespace quarkcache {

// A single cache entry holding the value and optional expiry timestamp.
struct CacheEntry {
    std::string value;
    std::optional<std::chrono::steady_clock::time_point> expires_at;

    bool is_expired() const {
        if (!expires_at) return false;
        return std::chrono::steady_clock::now() >= *expires_at;
    }

    // Seconds remaining until expiry; nullopt if no TTL.
    std::optional<int64_t> ttl_remaining() const {
        if (!expires_at) return std::nullopt;
        auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
            *expires_at - std::chrono::steady_clock::now()).count();
        return remaining > 0 ? std::make_optional(remaining) : std::make_optional<int64_t>(0);
    }
};

/**
 * LruCache — O(1) LRU eviction using a doubly-linked list + hash map.
 *
 * - std::list<string>            : recency order (front = MRU, back = LRU)
 * - std::unordered_map<key, MapValue> : O(1) lookup, holds list iterator + entry
 *
 * NOT thread-safe; callers (KVStore) must hold an external lock.
 */
class LruCache {
public:
    explicit LruCache(size_t capacity);

    bool set(const std::string& key,
             const std::string& value,
             std::optional<std::chrono::seconds> ttl = std::nullopt);

    std::optional<std::string> get(const std::string& key);

    bool del(const std::string& key);

    // Scan and remove all TTL-expired entries; returns count evicted.
    size_t evict_expired();

    size_t size() const { return map_.size(); }

    // Approximate heap bytes used by stored key+value strings.
    size_t bytes_used() const { return bytes_used_; }

    // Snapshot of all live (non-expired) entries.
    std::vector<std::pair<std::string, CacheEntry>> all_entries() const;

private:
    struct MapValue {
        std::list<std::string>::iterator order_it;
        CacheEntry entry;
    };

    // Promote entry to MRU (front of list).
    void touch(std::unordered_map<std::string, MapValue>::iterator it);

    // Evict the LRU (back of list) entry.
    void evict_lru();

    size_t capacity_;
    size_t bytes_used_{0};

    std::list<std::string> order_; // front = MRU, back = LRU
    std::unordered_map<std::string, MapValue> map_;
};

} // namespace quarkcache
