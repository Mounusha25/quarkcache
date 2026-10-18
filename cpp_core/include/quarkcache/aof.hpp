#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace quarkcache {

enum class AofOperation : uint8_t { Set = 1, Delete = 2 };
enum class FsyncPolicy { Always, EverySecond, No };

struct AofRecord {
    AofOperation operation;
    std::string key;
    std::string value;
    std::optional<std::chrono::system_clock::time_point> expires_at;
};

struct AofStats {
    uint64_t size_bytes{0};
    uint64_t writes{0};
    uint64_t fsyncs{0};
    uint64_t recovered_records{0};
    uint64_t truncated_bytes{0};
    uint64_t compactions{0};
    uint64_t reclaimed_bytes{0};
};

struct AofCompactionResult {
    uint64_t before_bytes{0};
    uint64_t after_bytes{0};
    uint64_t reclaimed_bytes{0};
    uint64_t records{0};
};

/**
 * Checksummed append-only mutation log.
 *
 * Each record is length framed and contains an absolute expiration timestamp,
 * allowing TTLs to retain their original deadline across process restarts.
 * A partial final record is treated as a crashed write and safely truncated;
 * checksum or structural corruption in a complete record fails startup.
 */
class AppendOnlyFile {
public:
    AppendOnlyFile(std::filesystem::path path, FsyncPolicy policy);
    ~AppendOnlyFile();

    AppendOnlyFile(const AppendOnlyFile&) = delete;
    AppendOnlyFile& operator=(const AppendOnlyFile&) = delete;

    void append_set(
        const std::string& key,
        const std::string& value,
        std::optional<std::chrono::system_clock::time_point> expires_at
    );
    void append_delete(const std::string& key);
    AofCompactionResult compact(const std::vector<AofRecord>& live_records);

    const std::vector<AofRecord>& recovered_records() const { return recovered_records_; }
    AofStats stats() const;
    const std::filesystem::path& path() const { return path_; }

private:
    void load_and_repair();
    void append(const AofRecord& record);
    void sync_if_needed();
    void sync_loop();

    std::filesystem::path path_;
    FsyncPolicy policy_;
    int fd_{-1};
    mutable std::mutex mutex_;
    std::condition_variable sync_condition_;
    std::thread sync_thread_;
    bool stopping_{false};
    bool dirty_{false};
    int background_errno_{0};
    std::vector<AofRecord> recovered_records_;
    AofStats stats_;
};

FsyncPolicy parse_fsync_policy(const std::string& value);

}  // namespace quarkcache
