#include "quarkcache/aof.hpp"
#include "quarkcache/kv_store.hpp"
#include "quarkcache/lru_cache.hpp"

#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;
using quarkcache::KVStore;
using quarkcache::LruCache;
using quarkcache::AppendOnlyFile;
using quarkcache::FsyncPolicy;

class TemporaryAof {
public:
    TemporaryAof() {
        static std::atomic<uint64_t> counter{0};
        path = std::filesystem::temp_directory_path() /
            ("quarkcache-test-" + std::to_string(++counter) + ".aof");
        std::filesystem::remove(path);
    }
    ~TemporaryAof() { std::filesystem::remove(path); }

    std::filesystem::path path;
};

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

void test_aof_round_trip_and_store_recovery() {
    TemporaryAof file;
    {
        auto aof = std::make_shared<AppendOnlyFile>(file.path, FsyncPolicy::Always);
        KVStore store(10, aof);
        store.restore();
        store.set("durable", "value");
        store.set("ttl", "temporary", std::chrono::seconds(60));
        store.set("deleted", "gone");
        store.del("deleted");
        expect(store.get_stats().aof_writes == 4, "mutations should be appended");
    }
    {
        auto aof = std::make_shared<AppendOnlyFile>(file.path, FsyncPolicy::No);
        KVStore recovered(10, aof);
        recovered.restore();

        expect(recovered.get("durable") == "value", "durable key should recover");
        expect(recovered.get("ttl") == "temporary", "unexpired TTL key should recover");
        expect(!recovered.get("deleted"), "deleted key should stay deleted");
        const auto stats = recovered.get_stats();
        expect(stats.aof_enabled, "AOF should be reported as enabled");
        expect(stats.aof_recovered_records == 4, "all complete records should replay");
    }
}

void test_aof_discards_incomplete_tail() {
    TemporaryAof file;
    {
        AppendOnlyFile aof(file.path, FsyncPolicy::Always);
        aof.append_set("safe", "value", std::nullopt);
    }
    {
        std::ofstream output(file.path, std::ios::binary | std::ios::app);
        output.write("bad", 3);
    }
    AppendOnlyFile recovered(file.path, FsyncPolicy::No);
    expect(recovered.recovered_records().size() == 1, "complete record should survive");
    expect(recovered.stats().truncated_bytes == 3, "partial tail should be reported");
}

void test_aof_rejects_checksum_corruption() {
    TemporaryAof file;
    {
        AppendOnlyFile aof(file.path, FsyncPolicy::Always);
        aof.append_set("key", "value", std::nullopt);
    }
    {
        std::fstream output(file.path, std::ios::binary | std::ios::in | std::ios::out);
        output.seekp(11);  // header (7) + frame length (4) = first payload byte
        const char corrupted = static_cast<char>(0x7f);
        output.write(&corrupted, 1);
    }
    bool threw = false;
    try {
        AppendOnlyFile corrupted(file.path, FsyncPolicy::No);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect(threw, "checksum corruption must fail startup");
}

void test_fsync_policy_validation() {
    expect(
        quarkcache::parse_fsync_policy("always") == FsyncPolicy::Always,
        "always policy should parse"
    );
    expect(
        quarkcache::parse_fsync_policy("everysec") == FsyncPolicy::EverySecond,
        "everysec policy should parse"
    );
    bool threw = false;
    try {
        (void)quarkcache::parse_fsync_policy("sometimes");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "unknown fsync policy should be rejected");
}

void test_everysec_policy_syncs_without_another_write() {
    TemporaryAof file;
    AppendOnlyFile aof(file.path, FsyncPolicy::EverySecond);
    aof.append_set("key", "value", std::nullopt);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (aof.stats().fsyncs == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    expect(aof.stats().fsyncs >= 1, "everysec worker should sync an idle dirty log");
}

void test_aof_compaction_preserves_only_live_state() {
    TemporaryAof file;
    uint64_t before_bytes = 0;
    {
        auto aof = std::make_shared<AppendOnlyFile>(file.path, FsyncPolicy::Always);
        KVStore store(10, aof);
        store.restore();
        for (int version = 0; version < 20; ++version) {
            store.set("hot", std::string(256, static_cast<char>('a' + version % 26)));
        }
        store.set("ttl", "survives", std::chrono::seconds(60));
        store.set("deleted", "gone");
        store.del("deleted");
        before_bytes = store.get_stats().aof_size_bytes;

        const auto result = store.compact_persistence();
        expect(result.before_bytes == before_bytes, "compaction should report original size");
        expect(result.after_bytes < result.before_bytes, "compaction should shrink stale history");
        expect(result.reclaimed_bytes == result.before_bytes - result.after_bytes,
               "reclaimed bytes should match the size reduction");
        expect(result.records == 2, "only two live keys should remain in the AOF");
        const auto stats = store.get_stats();
        expect(stats.aof_compactions == 1, "compaction should be counted");
        expect(stats.aof_reclaimed_bytes == result.reclaimed_bytes,
               "reclaimed bytes should be observable");
    }
    {
        auto aof = std::make_shared<AppendOnlyFile>(file.path, FsyncPolicy::No);
        KVStore recovered(10, aof);
        recovered.restore();
        expect(recovered.get("hot").has_value(), "latest overwritten value should recover");
        expect(recovered.get("ttl") == "survives", "live TTL value should recover");
        expect(!recovered.get("deleted"), "deleted key must not reappear after compaction");
        expect(recovered.get_stats().aof_recovered_records == 2,
               "compacted file should contain one record per live key");
    }
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
        {"AOF round trip", test_aof_round_trip_and_store_recovery},
        {"AOF truncated tail", test_aof_discards_incomplete_tail},
        {"AOF checksum", test_aof_rejects_checksum_corruption},
        {"fsync policy", test_fsync_policy_validation},
        {"everysec background sync", test_everysec_policy_syncs_without_another_write},
        {"AOF compaction", test_aof_compaction_preserves_only_live_state},
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
