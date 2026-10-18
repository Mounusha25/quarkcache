#include "quarkcache/aof.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace quarkcache {
namespace {

constexpr std::array<char, 7> kHeader{'Q', 'C', 'A', 'O', 'F', '1', '\n'};
constexpr uint32_t kMaxPayloadBytes = 64U * 1024U * 1024U;

void append_u32(std::vector<uint8_t>& output, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

void append_i64(std::vector<uint8_t>& output, int64_t value) {
    const auto bits = static_cast<uint64_t>(value);
    for (int shift = 0; shift < 64; shift += 8) {
        output.push_back(static_cast<uint8_t>((bits >> shift) & 0xffU));
    }
}

uint32_t read_u32(const std::vector<uint8_t>& input, size_t& offset) {
    if (offset + 4 > input.size()) throw std::runtime_error("invalid AOF record structure");
    uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        value |= static_cast<uint32_t>(input[offset++]) << shift;
    }
    return value;
}

int64_t read_i64(const std::vector<uint8_t>& input, size_t& offset) {
    if (offset + 8 > input.size()) throw std::runtime_error("invalid AOF record structure");
    uint64_t value = 0;
    for (int shift = 0; shift < 64; shift += 8) {
        value |= static_cast<uint64_t>(input[offset++]) << shift;
    }
    return static_cast<int64_t>(value);
}

uint32_t crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xffffffffU;
    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

void write_all(int fd, const uint8_t* data, size_t length) {
    size_t written = 0;
    while (written < length) {
        const ssize_t count = ::write(fd, data + written, length - written);
        if (count < 0) {
            if (errno == EINTR) continue;
            throw std::system_error(errno, std::generic_category(), "AOF write failed");
        }
        written += static_cast<size_t>(count);
    }
}

std::vector<uint8_t> encode(const AofRecord& record) {
    if (record.key.size() > std::numeric_limits<uint32_t>::max() ||
        record.value.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::length_error("AOF key or value is too large");
    }
    std::vector<uint8_t> payload;
    payload.reserve(17 + record.key.size() + record.value.size());
    payload.push_back(static_cast<uint8_t>(record.operation));
    int64_t expiry_ms = -1;
    if (record.expires_at) {
        expiry_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            record.expires_at->time_since_epoch()
        ).count();
    }
    append_i64(payload, expiry_ms);
    append_u32(payload, static_cast<uint32_t>(record.key.size()));
    append_u32(payload, static_cast<uint32_t>(record.value.size()));
    payload.insert(payload.end(), record.key.begin(), record.key.end());
    payload.insert(payload.end(), record.value.begin(), record.value.end());
    return payload;
}

std::vector<uint8_t> encode_frame(const AofRecord& record) {
    const auto payload = encode(record);
    std::vector<uint8_t> frame;
    frame.reserve(8 + payload.size());
    append_u32(frame, static_cast<uint32_t>(payload.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    append_u32(frame, crc32(payload.data(), payload.size()));
    return frame;
}

AofRecord decode(const std::vector<uint8_t>& payload) {
    if (payload.empty()) throw std::runtime_error("empty AOF record");
    size_t offset = 0;
    const auto operation = static_cast<AofOperation>(payload[offset++]);
    if (operation != AofOperation::Set && operation != AofOperation::Delete) {
        throw std::runtime_error("unknown AOF operation");
    }
    const int64_t expiry_ms = read_i64(payload, offset);
    const uint32_t key_length = read_u32(payload, offset);
    const uint32_t value_length = read_u32(payload, offset);
    if (offset + key_length + value_length != payload.size()) {
        throw std::runtime_error("invalid AOF record lengths");
    }
    AofRecord record;
    record.operation = operation;
    record.key.assign(reinterpret_cast<const char*>(payload.data() + offset), key_length);
    offset += key_length;
    record.value.assign(reinterpret_cast<const char*>(payload.data() + offset), value_length);
    if (expiry_ms >= 0) {
        record.expires_at = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(expiry_ms)
        );
    }
    if (operation == AofOperation::Delete && (!record.value.empty() || record.expires_at)) {
        throw std::runtime_error("invalid AOF delete record");
    }
    return record;
}

}  // namespace

AppendOnlyFile::AppendOnlyFile(std::filesystem::path path, FsyncPolicy policy)
    : path_(std::move(path)), policy_(policy) {
    load_and_repair();
    fd_ = ::open(path_.c_str(), O_WRONLY | O_APPEND);
    if (fd_ < 0) {
        throw std::system_error(errno, std::generic_category(), "could not open AOF");
    }
    if (policy_ == FsyncPolicy::EverySecond) {
        sync_thread_ = std::thread(&AppendOnlyFile::sync_loop, this);
    }
}

AppendOnlyFile::~AppendOnlyFile() {
    if (sync_thread_.joinable()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        sync_condition_.notify_one();
        sync_thread_.join();
    }
    if (fd_ >= 0) {
        if (policy_ != FsyncPolicy::No) ::fsync(fd_);
        ::close(fd_);
    }
}

void AppendOnlyFile::load_and_repair() {
    if (path_.has_parent_path()) std::filesystem::create_directories(path_.parent_path());
    if (!std::filesystem::exists(path_)) {
        std::ofstream created(path_, std::ios::binary);
        created.write(kHeader.data(), static_cast<std::streamsize>(kHeader.size()));
        created.close();
    }

    const uint64_t original_size = std::filesystem::file_size(path_);
    std::ifstream input(path_, std::ios::binary);
    std::array<char, kHeader.size()> header{};
    input.read(header.data(), static_cast<std::streamsize>(header.size()));
    if (input.gcount() != static_cast<std::streamsize>(header.size()) || header != kHeader) {
        throw std::runtime_error("invalid or unsupported AOF header");
    }
    uint64_t valid_size = kHeader.size();
    while (true) {
        const uint64_t record_start = valid_size;
        std::array<uint8_t, 4> length_bytes{};
        input.read(reinterpret_cast<char*>(length_bytes.data()), 4);
        if (input.gcount() == 0) break;
        if (input.gcount() != 4) {
            valid_size = record_start;
            break;
        }
        std::vector<uint8_t> length_vector(length_bytes.begin(), length_bytes.end());
        size_t length_offset = 0;
        const uint32_t payload_length = read_u32(length_vector, length_offset);
        if (payload_length == 0 || payload_length > kMaxPayloadBytes) {
            throw std::runtime_error("invalid AOF payload length");
        }
        std::vector<uint8_t> payload(payload_length);
        input.read(reinterpret_cast<char*>(payload.data()), payload_length);
        if (input.gcount() != static_cast<std::streamsize>(payload_length)) {
            valid_size = record_start;
            break;
        }
        std::array<uint8_t, 4> checksum_bytes{};
        input.read(reinterpret_cast<char*>(checksum_bytes.data()), 4);
        if (input.gcount() != 4) {
            valid_size = record_start;
            break;
        }
        std::vector<uint8_t> checksum_vector(checksum_bytes.begin(), checksum_bytes.end());
        size_t checksum_offset = 0;
        const uint32_t expected_checksum = read_u32(checksum_vector, checksum_offset);
        if (crc32(payload.data(), payload.size()) != expected_checksum) {
            throw std::runtime_error("AOF checksum mismatch");
        }
        recovered_records_.push_back(decode(payload));
        valid_size = record_start + 4 + payload_length + 4;
    }
    input.close();
    if (valid_size < original_size) {
        std::filesystem::resize_file(path_, valid_size);
        stats_.truncated_bytes = original_size - valid_size;
    }
    stats_.size_bytes = valid_size;
    stats_.recovered_records = recovered_records_.size();
}

void AppendOnlyFile::append_set(
    const std::string& key,
    const std::string& value,
    std::optional<std::chrono::system_clock::time_point> expires_at
) {
    append(AofRecord{AofOperation::Set, key, value, expires_at});
}

void AppendOnlyFile::append_delete(const std::string& key) {
    append(AofRecord{AofOperation::Delete, key, "", std::nullopt});
}

void AppendOnlyFile::append(const AofRecord& record) {
    const auto frame = encode_frame(record);

    std::lock_guard<std::mutex> lock(mutex_);
    if (background_errno_ != 0) {
        throw std::system_error(
            background_errno_, std::generic_category(), "background AOF fsync failed"
        );
    }
    write_all(fd_, frame.data(), frame.size());
    stats_.size_bytes += frame.size();
    ++stats_.writes;
    sync_if_needed();
}

AofCompactionResult AppendOnlyFile::compact(const std::vector<AofRecord>& live_records) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (background_errno_ != 0) {
        throw std::system_error(
            background_errno_, std::generic_category(), "background AOF fsync failed"
        );
    }

    const auto temporary_path = std::filesystem::path(
        path_.string() + ".rewrite." + std::to_string(::getpid())
    );
    const int temporary_fd = ::open(
        temporary_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644
    );
    if (temporary_fd < 0) {
        throw std::system_error(errno, std::generic_category(), "could not create compacted AOF");
    }

    uint64_t compacted_size = kHeader.size();
    bool installed = false;
    try {
        write_all(
            temporary_fd,
            reinterpret_cast<const uint8_t*>(kHeader.data()),
            kHeader.size()
        );
        for (const auto& record : live_records) {
            const auto frame = encode_frame(record);
            write_all(temporary_fd, frame.data(), frame.size());
            compacted_size += frame.size();
        }
        if (::fsync(temporary_fd) != 0) {
            throw std::system_error(errno, std::generic_category(), "compacted AOF fsync failed");
        }
        std::filesystem::rename(temporary_path, path_);

        const uint64_t previous_size = stats_.size_bytes;
        const int previous_fd = fd_;
        fd_ = temporary_fd;
        installed = true;
        ::close(previous_fd);
        dirty_ = false;
        stats_.size_bytes = compacted_size;
        ++stats_.compactions;
        const uint64_t reclaimed = previous_size > compacted_size
            ? previous_size - compacted_size
            : 0;
        stats_.reclaimed_bytes += reclaimed;

        const auto parent = path_.has_parent_path() ? path_.parent_path() : std::filesystem::path(".");
        const int directory_fd = ::open(parent.c_str(), O_RDONLY);
        if (directory_fd < 0) {
            background_errno_ = errno;
            throw std::system_error(errno, std::generic_category(), "could not open AOF directory");
        }
        const int directory_sync_result = ::fsync(directory_fd);
        const int directory_sync_errno = errno;
        ::close(directory_fd);
        if (directory_sync_result != 0) {
            background_errno_ = directory_sync_errno;
            throw std::system_error(
                directory_sync_errno, std::generic_category(), "AOF directory fsync failed"
            );
        }

        return AofCompactionResult{
            previous_size,
            compacted_size,
            reclaimed,
            static_cast<uint64_t>(live_records.size()),
        };
    } catch (...) {
        if (!installed) {
            ::close(temporary_fd);
            std::error_code cleanup_error;
            std::filesystem::remove(temporary_path, cleanup_error);
        }
        throw;
    }
}

void AppendOnlyFile::sync_if_needed() {
    dirty_ = true;
    if (policy_ == FsyncPolicy::Always) {
        if (::fsync(fd_) != 0) {
            throw std::system_error(errno, std::generic_category(), "AOF fsync failed");
        }
        ++stats_.fsyncs;
        dirty_ = false;
    }
}

void AppendOnlyFile::sync_loop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopping_) {
        sync_condition_.wait_for(lock, std::chrono::seconds(1), [this]() {
            return stopping_;
        });
        if (!dirty_) continue;
        if (::fsync(fd_) != 0) {
            background_errno_ = errno;
            stopping_ = true;
            break;
        }
        ++stats_.fsyncs;
        dirty_ = false;
    }
    if (dirty_ && background_errno_ == 0) {
        if (::fsync(fd_) != 0) {
            background_errno_ = errno;
        } else {
            ++stats_.fsyncs;
        }
        dirty_ = false;
    }
}

AofStats AppendOnlyFile::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

FsyncPolicy parse_fsync_policy(const std::string& value) {
    if (value == "always") return FsyncPolicy::Always;
    if (value == "everysec") return FsyncPolicy::EverySecond;
    if (value == "no") return FsyncPolicy::No;
    throw std::invalid_argument("QUARKCACHE_FSYNC must be always, everysec, or no");
}

}  // namespace quarkcache
