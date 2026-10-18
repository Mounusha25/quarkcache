/**
 * TcpServer — non-blocking event-driven TCP server.
 *
 * Platform dispatch:
 *   Linux  → epoll   (EPOLLET edge-triggered)
 *   macOS  → kqueue  (EV_ADD | EV_ENABLE)
 *
 * Both paths share the same command processing and per-client buffer logic.
 */

#include "quarkcache/server.hpp"
#include "quarkcache/lru_cache.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

// ── Platform-specific event API ──────────────────────────────────────────────
#if defined(__linux__)
#  include <sys/epoll.h>
#  define USE_EPOLL 1
#elif defined(__APPLE__) || defined(__FreeBSD__)
#  include <sys/event.h>
#  include <sys/time.h>
#  define USE_KQUEUE 1
#else
#  error "Unsupported platform: need Linux (epoll) or macOS/BSD (kqueue)"
#endif

#include <cstring>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace quarkcache {

namespace {

constexpr size_t kMaxInputBufferBytes = 1U * 1024U * 1024U;
constexpr size_t kOutputHighWaterBytes = 64U * 1024U;
constexpr size_t kMaxOutputBufferBytes = 1U * 1024U * 1024U;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) throw std::runtime_error("fcntl F_GETFL failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl F_SETFL O_NONBLOCK failed");
}

std::vector<std::string> tokenize(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

TcpServer::TcpServer(std::shared_ptr<KVStore> store,
                     const std::string& host,
                     uint16_t port)
    : store_(std::move(store)),
      host_(host),
      port_(port) {}

TcpServer::~TcpServer() {
    stop();
    for (const auto& [fd, state] : clients_) {
        (void)state;
        close(fd);
        store_->decrement_connections();
    }
    clients_.clear();
    if (listen_fd_ != -1) { close(listen_fd_); listen_fd_ = -1; }
    if (epoll_fd_  != -1) { close(epoll_fd_);  epoll_fd_  = -1; }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public interface
// ─────────────────────────────────────────────────────────────────────────────

void TcpServer::setup_listen_socket() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == -1) throw std::runtime_error("socket() failed");

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0)
        throw std::runtime_error("Invalid host: " + host_);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
        throw std::runtime_error(std::string("bind() failed: ") + strerror(errno));

    if (listen(listen_fd_, SOMAXCONN) == -1)
        throw std::runtime_error("listen() failed");

    set_nonblocking(listen_fd_);
}

void TcpServer::run() {
    setup_listen_socket();

#if USE_EPOLL
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) throw std::runtime_error("epoll_create1() failed");

    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = listen_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev);

#elif USE_KQUEUE
    epoll_fd_ = kqueue();   // we reuse the epoll_fd_ member for the kqueue fd
    if (epoll_fd_ == -1) throw std::runtime_error("kqueue() failed");

    struct kevent kev{};
    EV_SET(&kev, listen_fd_, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    if (kevent(epoll_fd_, &kev, 1, nullptr, 0, nullptr) == -1)
        throw std::runtime_error("kevent register listen_fd failed");
#endif

    running_ = true;
    std::cout << "[QuarkCache] Listening on " << host_ << ":" << port_ << "\n";
    event_loop();
}

void TcpServer::stop() { running_ = false; }

// ─────────────────────────────────────────────────────────────────────────────
//  Event loop (platform-dispatched)
// ─────────────────────────────────────────────────────────────────────────────

void TcpServer::event_loop() {
    constexpr int MAX_EVENTS = 64;

#if USE_EPOLL
    epoll_event events[MAX_EVENTS];
    while (running_) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100 /*ms*/);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "[QuarkCache] epoll_wait: " << strerror(errno) << "\n";
            break;
        }
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd == listen_fd_) {
                accept_client(epoll_fd_);
            } else if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                close_client(epoll_fd_, fd);
            } else {
                if (events[i].events & EPOLLIN) handle_read(epoll_fd_, fd);
                if (has_client(fd) && (events[i].events & EPOLLOUT)) {
                    handle_write(epoll_fd_, fd);
                }
            }
        }
    }

#elif USE_KQUEUE
    struct kevent events[MAX_EVENTS];
    struct timespec timeout{ 0, 100'000'000 }; // 100 ms

    while (running_) {
        int n = kevent(epoll_fd_, nullptr, 0, events, MAX_EVENTS, &timeout);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "[QuarkCache] kevent: " << strerror(errno) << "\n";
            break;
        }
        for (int i = 0; i < n; ++i) {
            int fd = static_cast<int>(events[i].ident);
            if (events[i].flags & EV_ERROR) {
                std::cerr << "[QuarkCache] EV_ERROR on fd=" << fd << "\n";
                if (fd != listen_fd_) close_client(epoll_fd_, fd);
                continue;
            }
            if (fd == listen_fd_) {
                accept_client(epoll_fd_);
            } else if (events[i].filter == EVFILT_READ) {
                handle_read(epoll_fd_, fd);
            } else if (has_client(fd) && events[i].filter == EVFILT_WRITE) {
                handle_write(epoll_fd_, fd);
            }
        }
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  Accept / read helpers
// ─────────────────────────────────────────────────────────────────────────────

void TcpServer::accept_client(int event_fd) {
    while (true) {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        int cfd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len);
        if (cfd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::cerr << "[QuarkCache] accept: " << strerror(errno) << "\n";
            break;
        }
        set_nonblocking(cfd);
        store_->increment_connections();

#if USE_EPOLL
        epoll_event ev{};
        ev.events  = EPOLLIN | EPOLLRDHUP | EPOLLET;
        ev.data.fd = cfd;
        epoll_ctl(event_fd, EPOLL_CTL_ADD, cfd, &ev);

#elif USE_KQUEUE
        struct kevent changes[2]{};
        EV_SET(&changes[0], cfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
        EV_SET(&changes[1], cfd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, nullptr);
        kevent(event_fd, changes, 2, nullptr, 0, nullptr);
#endif

        clients_.emplace(cfd, ClientState{});
        std::cout << "[QuarkCache] Client connected: fd=" << cfd << "\n";
    }
}

void TcpServer::handle_read(int event_fd, int client_fd) {
    char buf[4096];

    while (true) {
        ssize_t n = read(client_fd, buf, sizeof(buf));
        if (n > 0) {
            auto it = clients_.find(client_fd);
            if (it == clients_.end()) return;
            it->second.input.append(buf, static_cast<size_t>(n));
            if (it->second.input.size() > kMaxInputBufferBytes) {
                it->second.input.clear();
                it->second.read_paused = true;
                it->second.close_after_write = true;
                queue_response(event_fd, client_fd, "ERROR input buffer limit exceeded\r\n");
                handle_write(event_fd, client_fd);
                return;
            }
            if (!process_buffered_commands(event_fd, client_fd)) return;
            if (clients_.at(client_fd).read_paused) {
                clients_.at(client_fd).kernel_read_deferred = true;
                break;
            }
        } else if (n == 0) {
            std::cout << "[QuarkCache] Client disconnected: fd=" << client_fd << "\n";
            close_client(event_fd, client_fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::cerr << "[QuarkCache] read error fd=" << client_fd
                      << ": " << strerror(errno) << "\n";
            close_client(event_fd, client_fd);
            return;
        }
    }
}

bool TcpServer::process_buffered_commands(int event_fd, int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) return false;

    size_t consumed = 0;
    uint64_t batch_commands = 0;
    while (!it->second.read_paused) {
        const size_t newline = it->second.input.find('\n', consumed);
        if (newline == std::string::npos) break;
        std::string line = it->second.input.substr(consumed, newline - consumed);
        consumed = newline + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        ++batch_commands;
        ++commands_processed_;
        if (!queue_response(event_fd, client_fd, process_command(line, client_fd))) {
            return false;
        }
        it = clients_.find(client_fd);
        if (it == clients_.end()) return false;
    }

    if (consumed > 0) it->second.input.erase(0, consumed);
    if (batch_commands > 1) pipelined_commands_ += batch_commands;
    update_interest(event_fd, client_fd);
    return true;
}

bool TcpServer::queue_response(int event_fd, int client_fd, std::string response) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) return false;
    auto& state = it->second;
    const size_t pending = state.output.size() - state.output_offset;

    if (pending + response.size() > kMaxOutputBufferBytes) {
        static const std::string limit_error = "ERROR output buffer limit exceeded\r\n";
        if (pending + limit_error.size() > kMaxOutputBufferBytes) {
            close_client(event_fd, client_fd);
            return false;
        }
        response = limit_error;
        state.close_after_write = true;
    }

    state.output.append(response);
    pending_output_bytes_ += response.size();
    const size_t updated_pending = state.output.size() - state.output_offset;
    if (updated_pending >= kOutputHighWaterBytes && !state.read_paused) {
        state.read_paused = true;
        ++backpressure_events_;
    }
    update_interest(event_fd, client_fd);
    return true;
}

void TcpServer::handle_write(int event_fd, int client_fd) {
    while (true) {
        auto it = clients_.find(client_fd);
        if (it == clients_.end()) return;
        auto& state = it->second;

        if (state.output_offset == state.output.size()) {
            state.output.clear();
            state.output_offset = 0;
            if (state.close_after_write) {
                close_client(event_fd, client_fd);
                return;
            }

            state.read_paused = false;
            if (!process_buffered_commands(event_fd, client_fd)) return;
            it = clients_.find(client_fd);
            if (it == clients_.end()) return;
            if (it->second.output.empty()) {
                const bool drain_deferred_read = it->second.kernel_read_deferred;
                it->second.kernel_read_deferred = false;
                update_interest(event_fd, client_fd);
                if (drain_deferred_read) handle_read(event_fd, client_fd);
                return;
            }
            continue;
        }

        const size_t remaining = state.output.size() - state.output_offset;
#ifdef MSG_NOSIGNAL
        const ssize_t written = send(
            client_fd, state.output.data() + state.output_offset, remaining, MSG_NOSIGNAL
        );
#else
        const ssize_t written = write(
            client_fd, state.output.data() + state.output_offset, remaining
        );
#endif
        if (written > 0) {
            const auto count = static_cast<size_t>(written);
            state.output_offset += count;
            pending_output_bytes_ -= count;
            if (count < remaining) ++partial_writes_;
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            ++partial_writes_;
            update_interest(event_fd, client_fd);
            return;
        }
        close_client(event_fd, client_fd);
        return;
    }
}

void TcpServer::update_interest(int event_fd, int client_fd) {
    const auto it = clients_.find(client_fd);
    if (it == clients_.end()) return;
    const auto& state = it->second;
    const bool wants_read = !state.read_paused && !state.close_after_write;
    const bool wants_write = state.output_offset < state.output.size();

#if USE_EPOLL
    epoll_event event{};
    event.events = EPOLLRDHUP | EPOLLET;
    if (wants_read) event.events |= EPOLLIN;
    if (wants_write) event.events |= EPOLLOUT;
    event.data.fd = client_fd;
    if (epoll_ctl(event_fd, EPOLL_CTL_MOD, client_fd, &event) == -1 && errno != ENOENT) {
        std::cerr << "[QuarkCache] epoll_ctl MOD fd=" << client_fd
                  << ": " << strerror(errno) << "\n";
    }
#elif USE_KQUEUE
    struct kevent changes[2]{};
    EV_SET(
        &changes[0], client_fd, EVFILT_READ,
        EV_ADD | (wants_read ? EV_ENABLE : EV_DISABLE), 0, 0, nullptr
    );
    EV_SET(
        &changes[1], client_fd, EVFILT_WRITE,
        EV_ADD | (wants_write ? EV_ENABLE : EV_DISABLE), 0, 0, nullptr
    );
    if (kevent(event_fd, changes, 2, nullptr, 0, nullptr) == -1 && errno != ENOENT) {
        std::cerr << "[QuarkCache] kevent update fd=" << client_fd
                  << ": " << strerror(errno) << "\n";
    }
#endif
}

void TcpServer::close_client(int event_fd, int client_fd) {
    const auto it = clients_.find(client_fd);
    if (it == clients_.end()) return;
    const size_t pending = it->second.output.size() - it->second.output_offset;
    pending_output_bytes_ -= pending;

#if USE_EPOLL
    epoll_ctl(event_fd, EPOLL_CTL_DEL, client_fd, nullptr);
#elif USE_KQUEUE
    struct kevent changes[2]{};
    EV_SET(&changes[0], client_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&changes[1], client_fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    kevent(event_fd, changes, 2, nullptr, 0, nullptr);
#endif
    close(client_fd);
    clients_.erase(it);
    store_->decrement_connections();
}

bool TcpServer::has_client(int client_fd) const {
    return clients_.find(client_fd) != clients_.end();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Command dispatch
// ─────────────────────────────────────────────────────────────────────────────

std::string TcpServer::process_command(const std::string& line, int /*client_fd*/) {
    auto tokens = tokenize(line);
    if (tokens.empty()) return "ERROR empty command\r\n";

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    if (cmd == "SET") {
        if (tokens.size() < 3) return "ERROR SET <key> <value> [<ttl>]\r\n";
        std::optional<std::chrono::seconds> ttl;
        if (tokens.size() >= 4) {
            try { ttl = std::chrono::seconds(std::stoll(tokens[3])); }
            catch (...) { return "ERROR invalid TTL\r\n"; }
        }
        store_->set(tokens[1], tokens[2], ttl);
        return "OK\r\n";

    } else if (cmd == "GET") {
        if (tokens.size() < 2) return "ERROR GET <key>\r\n";
        auto val = store_->get(tokens[1]);
        return val ? "VALUE " + *val + "\r\n" : "NOT_FOUND\r\n";

    } else if (cmd == "DEL") {
        if (tokens.size() < 2) return "ERROR DEL <key>\r\n";
        return store_->del(tokens[1]) ? "OK\r\n" : "NOT_FOUND\r\n";

    } else if (cmd == "STATS") {
        Stats s = store_->get_stats();
        std::ostringstream oss;
        oss << "STATS {"
            << "\"total_requests\":"    << s.total_requests
            << ",\"hits\":"             << s.hits
            << ",\"misses\":"           << s.misses
            << ",\"bytes_used\":"       << s.bytes_used
            << ",\"active_connections\":" << s.active_connections
            << ",\"aof_enabled\":"       << (s.aof_enabled ? "true" : "false")
            << ",\"aof_size_bytes\":"    << s.aof_size_bytes
            << ",\"aof_writes\":"        << s.aof_writes
            << ",\"aof_fsyncs\":"        << s.aof_fsyncs
            << ",\"aof_recovered_records\":" << s.aof_recovered_records
            << ",\"aof_truncated_bytes\":" << s.aof_truncated_bytes
            << ",\"aof_compactions\":"     << s.aof_compactions
            << ",\"aof_reclaimed_bytes\":" << s.aof_reclaimed_bytes
            << ",\"network_commands_processed\":" << commands_processed_.load()
            << ",\"network_pipelined_commands\":" << pipelined_commands_.load()
            << ",\"network_partial_writes\":" << partial_writes_.load()
            << ",\"network_backpressure_events\":" << backpressure_events_.load()
            << ",\"network_pending_output_bytes\":" << pending_output_bytes_.load()
            << "}\r\n";
        return oss.str();

    } else if (cmd == "COMPACT") {
        if (tokens.size() != 1) return "ERROR COMPACT takes no arguments\r\n";
        try {
            const auto result = store_->compact_persistence();
            std::ostringstream oss;
            oss << "COMPACT {"
                << "\"before_bytes\":"   << result.before_bytes
                << ",\"after_bytes\":"    << result.after_bytes
                << ",\"reclaimed_bytes\":" << result.reclaimed_bytes
                << ",\"records\":"        << result.records
                << "}\r\n";
            return oss.str();
        } catch (const std::logic_error&) {
            return "ERROR AOF persistence is disabled\r\n";
        } catch (const std::exception& error) {
            std::cerr << "[QuarkCache] AOF compaction failed: " << error.what() << "\n";
            return "ERROR AOF compaction failed\r\n";
        }

    } else if (cmd == "KEYS") {
        std::ostringstream oss;
        {
            std::lock_guard<std::mutex> lock(store_->mutex());
            for (const auto& [k, entry] : store_->lru()->all_entries()) {
                auto ttl_rem = entry.ttl_remaining();
                oss << k << " " << entry.value << " "
                    << (ttl_rem ? std::to_string(*ttl_rem) : "-1") << "\r\n";
            }
        }
        oss << "END\r\n";
        return oss.str();

    } else {
        return "ERROR unknown command '" + cmd + "'\r\n";
    }
}

} // namespace quarkcache
