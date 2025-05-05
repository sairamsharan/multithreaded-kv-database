#include <iostream>
#include <thread>
#include <chrono>
#include "kv_store.hpp"
#include "thread_pool.hpp"
#include "wal.hpp"
#include "tcp_server.hpp"

// Background thread for TTL cleanup
void ttl_cleaner_loop(KVStore& store) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        store.remove_expired_keys();
    }
}

int main() {
    std::cout << "Starting Multithreaded In-Memory KV Store..." << std::endl;

    // 1. Initialize Thread Pool (8 workers)
    ThreadPool pool(8);

    // 2. Initialize Key-Value Store (Max 100,000 keys)
    KVStore store(100000);

    // 3. Initialize Write-Ahead Log
    WriteAheadLog wal("wal.log");

    // 4. Replay WAL on startup to restore state
    std::cout << "Replaying WAL..." << std::endl;
    wal.replay(store);
    std::cout << "WAL replay complete." << std::endl;

    // 5. Start background TTL cleaner thread
    std::thread ttl_thread(ttl_cleaner_loop, std::ref(store));
    ttl_thread.detach(); // Let it run independently

    // 6. Initialize and start TCP Server
    // Listen on 0.0.0.0 (all interfaces) port 6379 (standard redis port)
    TCPServer server("0.0.0.0", 6379, 4096, pool, store, wal);
    
    // server.start() is a blocking call that accepts connections in an infinite loop
    server.start();

    return 0;
}
