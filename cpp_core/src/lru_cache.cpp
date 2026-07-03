#include "quarkcache/lru_cache.hpp"
#include <stdexcept>

namespace quarkcache {

LruCache::LruCache(size_t capacity) : capacity_(capacity) {
    if (capacity == 0) throw std::invalid_argument("LruCache capacity must be > 0");
}

bool LruCache::set(const std::string& key,
                   const std::string& value,
                   std::optional<std::chrono::seconds> ttl) {
    auto it = map_.find(key);
    if (it != map_.end()) {
        // Update in-place: adjust byte accounting, move to MRU position.
        bytes_used_ -= it->first.size() + it->second.entry.value.size();
        it->second.entry.value = value;
        it->second.entry.expires_at =
            ttl ? std::make_optional(std::chrono::steady_clock::now() + *ttl)
                : std::nullopt;
        bytes_used_ += key.size() + value.size();
        touch(it);
        return true;
    }

    // Evict LRU entries until below capacity.
    while (map_.size() >= capacity_) {
        evict_lru();
    }

    order_.push_front(key);
    CacheEntry entry;
    entry.value = value;
    entry.expires_at = ttl
        ? std::make_optional(std::chrono::steady_clock::now() + *ttl)
        : std::nullopt;

    map_.emplace(key, MapValue{order_.begin(), std::move(entry)});
    bytes_used_ += key.size() + value.size();
    return true;
}

std::optional<std::string> LruCache::get(const std::string& key) {
    auto it = map_.find(key);
    if (it == map_.end()) return std::nullopt;

    if (it->second.entry.is_expired()) {
        bytes_used_ -= it->first.size() + it->second.entry.value.size();
        order_.erase(it->second.order_it);
        map_.erase(it);
        return std::nullopt;
    }

    touch(it);
    return it->second.entry.value;
}

bool LruCache::del(const std::string& key) {
    auto it = map_.find(key);
    if (it == map_.end()) return false;

    bytes_used_ -= it->first.size() + it->second.entry.value.size();
    order_.erase(it->second.order_it);
    map_.erase(it);
    return true;
}

size_t LruCache::evict_expired() {
    size_t count = 0;
    auto it = map_.begin();
    while (it != map_.end()) {
        if (it->second.entry.is_expired()) {
            bytes_used_ -= it->first.size() + it->second.entry.value.size();
            order_.erase(it->second.order_it);
            it = map_.erase(it);
            ++count;
        } else {
            ++it;
        }
    }
    return count;
}

std::vector<std::pair<std::string, CacheEntry>> LruCache::all_entries() const {
    std::vector<std::pair<std::string, CacheEntry>> result;
    result.reserve(map_.size());
    for (const auto& [k, mv] : map_) {
        if (!mv.entry.is_expired()) {
            result.emplace_back(k, mv.entry);
        }
    }
    return result;
}

void LruCache::touch(std::unordered_map<std::string, MapValue>::iterator it) {
    order_.splice(order_.begin(), order_, it->second.order_it);
    it->second.order_it = order_.begin();
}

void LruCache::evict_lru() {
    if (order_.empty()) return;
    const std::string lru_key = order_.back();
    auto it = map_.find(lru_key);
    if (it != map_.end()) {
        bytes_used_ -= it->first.size() + it->second.entry.value.size();
        map_.erase(it);
    }
    order_.pop_back();
}

} // namespace quarkcache
