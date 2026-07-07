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
            } else if (events[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR)) {
                handle_read(epoll_fd_, fd);
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
                continue;
            }
            if (fd == listen_fd_) {
                accept_client(epoll_fd_);
            } else {
                handle_read(epoll_fd_, fd);
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
        ev.events  = EPOLLIN | EPOLLET;
        ev.data.fd = cfd;
        epoll_ctl(event_fd, EPOLL_CTL_ADD, cfd, &ev);

#elif USE_KQUEUE
        struct kevent kev{};
        EV_SET(&kev, cfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
        kevent(event_fd, &kev, 1, nullptr, 0, nullptr);
#endif

        {
            std::lock_guard<std::mutex> lock(buffers_mu_);
            read_buffers_[cfd] = "";
        }
        std::cout << "[QuarkCache] Client connected: fd=" << cfd << "\n";
    }
}

void TcpServer::handle_read(int event_fd, int client_fd) {
    char buf[4096];
    std::string accumulated;

    while (true) {
        ssize_t n = read(client_fd, buf, sizeof(buf));
        if (n > 0) {
            accumulated.append(buf, static_cast<size_t>(n));
        } else if (n == 0) {
            // Clean disconnect.
            std::cout << "[QuarkCache] Client disconnected: fd=" << client_fd << "\n";

#if USE_EPOLL
            epoll_ctl(event_fd, EPOLL_CTL_DEL, client_fd, nullptr);
#elif USE_KQUEUE
            struct kevent kev{};
            EV_SET(&kev, client_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
            kevent(event_fd, &kev, 1, nullptr, 0, nullptr);
#endif
            close(client_fd);
            store_->decrement_connections();
            {
                std::lock_guard<std::mutex> lock(buffers_mu_);
                read_buffers_.erase(client_fd);
            }
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::cerr << "[QuarkCache] read error fd=" << client_fd
                      << ": " << strerror(errno) << "\n";
#if USE_EPOLL
            epoll_ctl(event_fd, EPOLL_CTL_DEL, client_fd, nullptr);
#elif USE_KQUEUE
            struct kevent kev{};
            EV_SET(&kev, client_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
            kevent(event_fd, &kev, 1, nullptr, 0, nullptr);
#endif
            close(client_fd);
            store_->decrement_connections();
            {
                std::lock_guard<std::mutex> lock(buffers_mu_);
                read_buffers_.erase(client_fd);
            }
            return;
        }
    }

    if (accumulated.empty()) return;

    std::string* client_buf;
    {
        std::lock_guard<std::mutex> lock(buffers_mu_);
        client_buf = &read_buffers_[client_fd];
        *client_buf += accumulated;
    }

    // Process all complete \n-terminated lines from the buffer.
    std::lock_guard<std::mutex> lock(buffers_mu_);
    std::string& buf_ref = read_buffers_[client_fd];
    size_t pos;
    while ((pos = buf_ref.find('\n')) != std::string::npos) {
        std::string line = buf_ref.substr(0, pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        buf_ref.erase(0, pos + 1);
        if (line.empty()) continue;

        std::string response = process_command(line, client_fd);
        if (!response.empty()) {
            ssize_t written = write(client_fd, response.data(), response.size());
            (void)written; // best-effort; production code would buffer partial writes
        }
    }
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
            << "}\r\n";
        return oss.str();

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
