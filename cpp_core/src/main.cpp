#include "quarkcache/kv_store.hpp"
#include "quarkcache/server.hpp"

#include <iostream>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

// Global so the signal handler can stop the server cleanly.
static quarkcache::TcpServer* g_server = nullptr;

void handle_signal(int) {
    std::cout << "\n[QuarkCache] Shutting down...\n";
    if (g_server) g_server->stop();
}

int main() {
    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    // Shared store with up to 100 000 keys.
    auto store = std::make_shared<quarkcache::KVStore>(100'000);

    // Background thread: prune TTL-expired keys every 5 seconds.
    std::thread expiry_thread([&store]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            store->evict_expired();
        }
    });
    expiry_thread.detach();

    const char* host_env = std::getenv("QUARKCACHE_BIND_HOST");
    const char* port_env = std::getenv("QUARKCACHE_PORT");
    const std::string host = host_env ? host_env : "0.0.0.0";
    const auto port = static_cast<uint16_t>(port_env ? std::stoi(port_env) : 9000);

    quarkcache::TcpServer server(store, host, port);
    g_server = &server;

    try {
        server.run(); // blocks until stop() is called
    } catch (const std::exception& ex) {
        std::cerr << "[QuarkCache] Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
