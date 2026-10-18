#include "quarkcache/kv_store.hpp"
#include "quarkcache/lru_cache.hpp"

#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;
using quarkcache::KVStore;
using quarkcache::LruCache;

void expect(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void test_rejects_zero_capacity() {
    bool threw = false;
    try {
        LruCache cache(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "zero capacity must be rejected");
}

void test_set_get_update_and_bytes() {
    LruCache cache(2);
    expect(cache.set("alpha", "one"), "SET should succeed");
    expect(cache.get("alpha") == "one", "GET should return stored value");
    expect(cache.bytes_used() == 8, "byte accounting should include key and value");

    cache.set("alpha", "three");
    expect(cache.get("alpha") == "three", "SET should update existing value");
    expect(cache.bytes_used() == 10, "byte accounting should update with value");
}

void test_lru_eviction_respects_recent_access() {
    LruCache cache(2);
    cache.set("a", "1");
    cache.set("b", "2");
    expect(cache.get("a") == "1", "access should find a");
    cache.set("c", "3");

    expect(!cache.get("b"), "least-recently-used key should be evicted");
    expect(cache.get("a") == "1", "recently accessed key should remain");
    expect(cache.get("c") == "3", "new key should remain");
}

void test_delete_and_expiration() {
    LruCache cache(3);
    cache.set("delete-me", "value");
    expect(cache.del("delete-me"), "existing key should be deleted");
    expect(!cache.del("delete-me"), "deleting missing key should return false");

    cache.set("expired", "value", std::chrono::seconds(0));
    expect(!cache.get("expired"), "zero-TTL key should expire immediately");
    expect(cache.size() == 0, "expired key should be removed on access");

    cache.set("sweep", "value", std::chrono::seconds(0));
    expect(cache.evict_expired() == 1, "expiry sweep should report removed key");
}

void test_snapshot_excludes_expired_entries() {
    LruCache cache(3);
    cache.set("live", "value");
    cache.set("expired", "value", std::chrono::seconds(0));
    const auto entries = cache.all_entries();

    expect(entries.size() == 1, "snapshot should contain only live entries");
    expect(entries.front().first == "live", "snapshot should contain live key");
}

void test_store_metrics() {
    KVStore store(4);
    store.set("key", "value");
    expect(store.get("key") == "value", "store hit should return value");
    expect(!store.get("missing"), "store miss should be empty");
    expect(store.del("key"), "store delete should succeed");

    const auto stats = store.get_stats();
    expect(stats.total_requests == 4, "all operations should be counted");
    expect(stats.hits == 1, "hit should be counted");
    expect(stats.misses == 1, "miss should be counted");
    expect(stats.bytes_used == 0, "deleted value should release accounted bytes");
}

void test_concurrent_store_access() {
    constexpr int thread_count = 8;
    constexpr int operations_per_thread = 500;
    KVStore store(thread_count * operations_per_thread);
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;

    for (int thread_id = 0; thread_id < thread_count; ++thread_id) {
        threads.emplace_back([&store, &failures, thread_id]() {
            for (int index = 0; index < operations_per_thread; ++index) {
                const auto key = std::to_string(thread_id) + ":" + std::to_string(index);
                store.set(key, "value");
                if (store.get(key) != "value") ++failures;
            }
        });
    }
    for (auto& thread : threads) thread.join();

    expect(failures == 0, "concurrent SET/GET operations should not lose values");
    const auto stats = store.get_stats();
    expect(
        stats.total_requests == thread_count * operations_per_thread * 2,
        "concurrent operations should be counted"
    );
    expect(stats.misses == 0, "concurrent round trips should not miss");
}

}  // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"rejects zero capacity", test_rejects_zero_capacity},
        {"set, get, update, and bytes", test_set_get_update_and_bytes},
        {"LRU eviction", test_lru_eviction_respects_recent_access},
        {"delete and expiration", test_delete_and_expiration},
        {"snapshot expiration", test_snapshot_excludes_expired_entries},
        {"store metrics", test_store_metrics},
        {"concurrent access", test_concurrent_store_access},
    };

    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - failures << "/" << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
