#pragma once

#include "kv_store.hpp"
#include <string>
#include <memory>
#include <atomic>
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
    void handle_write(int event_fd, int client_fd);
    bool process_buffered_commands(int event_fd, int client_fd);
    bool queue_response(int event_fd, int client_fd, std::string response);
    void update_interest(int event_fd, int client_fd);
    void close_client(int event_fd, int client_fd);
    bool has_client(int client_fd) const;
    std::string process_command(const std::string& line, int client_fd);

    struct ClientState {
        std::string input;
        std::string output;
        size_t output_offset{0};
        bool read_paused{false};
        bool kernel_read_deferred{false};
        bool close_after_write{false};
    };

    std::shared_ptr<KVStore> store_;
    std::string host_;
    uint16_t port_;
    int listen_fd_{-1};
    int epoll_fd_{-1}; // epoll fd on Linux, kqueue fd on macOS

    std::atomic<bool> running_{false};

    // The event loop owns all client state; no cross-thread access occurs.
    std::unordered_map<int, ClientState> clients_;
    std::atomic<uint64_t> commands_processed_{0};
    std::atomic<uint64_t> pipelined_commands_{0};
    std::atomic<uint64_t> partial_writes_{0};
    std::atomic<uint64_t> backpressure_events_{0};
    std::atomic<uint64_t> pending_output_bytes_{0};
};

} // namespace quarkcache
