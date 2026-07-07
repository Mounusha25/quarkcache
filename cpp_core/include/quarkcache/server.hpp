#pragma once

#include "kv_store.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace quarkcache {

/**
 * TcpServer — non-blocking event-driven TCP server.
 *
 * Uses epoll on Linux, kqueue on macOS/BSD.
 *
 * Protocol (line-delimited, CRLF-terminated):
 *   SET <key> <value> [<ttl_seconds>]\r\n  →  OK | ERROR
 *   GET <key>\r\n                           →  VALUE <val> | NOT_FOUND | ERROR
 *   DEL <key>\r\n                           →  OK | NOT_FOUND | ERROR
 *   STATS\r\n                               →  STATS <json>
 *   KEYS\r\n                                →  key value ttl\r\n … END\r\n
 */
class TcpServer {
public:
    TcpServer(std::shared_ptr<KVStore> store,
              const std::string& host = "127.0.0.1",
              uint16_t port = 9000);

    ~TcpServer();

    // Blocks until stop() is called from another thread or process receives SIGINT.
    void run();

    void stop();

private:
    void event_loop();
    void setup_listen_socket();
    void accept_client(int event_fd);
    void handle_read(int event_fd, int client_fd);
    std::string process_command(const std::string& line, int client_fd);

    std::shared_ptr<KVStore> store_;
    std::string host_;
    uint16_t port_;
    int listen_fd_{-1};
    int epoll_fd_{-1}; // epoll fd on Linux, kqueue fd on macOS

    std::atomic<bool> running_{false};

    // Per-client read buffers (fd → accumulated partial data).
    std::unordered_map<int, std::string> read_buffers_;
    std::mutex buffers_mu_;
};

} // namespace quarkcache
