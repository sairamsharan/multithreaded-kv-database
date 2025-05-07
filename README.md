# Multithreaded In-Memory Key-Value Database

A high-performance, Redis-like, concurrent in-memory Key-Value database written in C++ (C++20). Engineered with raw TCP sockets to handle concurrent client requests efficiently using a custom bounded Thread Pool.

Designed with scalability in mind, this database implements fine-grained **lock striping** across 128 distinct shards to maximize throughput, bypasses single-mutex bottlenecks, and integrates durable storage via a **Write-Ahead Log (WAL)**.

### 🚀 Core Features

- **Custom Thread Pool:** A fixed-size, bounded thread pool dynamically scales workloads using condition variable-backed queues, preventing thread-thrashing and OS overhead under intense load.
- **Lock Striping Engine:** The dataset is partitioned into 128 `std::shared_mutex` shards, enabling true multi-core parallel reads/writes across independent keys.
- **LRU & TTL Eviction Policies:** Incorporates O(1) Least-Recently-Used (LRU) eviction through Hashmap & Doubly Linked List combinations and a background sweeping thread that purges expired TTL nodes to enforce strict memory constraints.
- **Write-Ahead Log (WAL):** Enables crash recovery via an append-only transaction state ledger that replays commands on startup.
- **TCP Networking Layer:** Uses raw sockets (`Winsock` / POSIX `socket()`, `bind()`, `listen()`) to maintain low-level stream connections for clients.

### 📈 Benchmarks

A load-generator simulating concurrent users executing mixed text protocol requests (`SET`, `GET`, `DEL`) was built alongside the server.

| Metric | Details |
| :--- | :--- |
| **Concurrency Level** | 1000 Simultaneous Clients |
| **Data Throughput** | 100,000 Operations |
| **Throughput (Ops/Sec)**| ~25,000 Ops/Sec (Hardware-dependent) |

**Result:** *Achieved stable throughput under 1000 concurrent clients with minimal latency, effectively utilizing multiple cores with reduced contention due to shard-based locking.*

## ⚙️ Compilation & Setup

Built strictly with C++ standard libraries (No external UI/UX or HTTP frameworks).

### CMake Build (Windows / Linux)
```bash
# Generate build configuration
cmake -S . -B build

# Compile Executables
cd build
g++ -std=c++20 -DNOMINMAX -DWIN32_LEAN_AND_MEAN -o server.build ../src/main.cpp ../src/kv_store.cpp ../src/thread_pool.cpp ../src/wal.cpp ../src/tcp_server.cpp -lws2_32 -pthread
```
*(On CMake natively, simply `make` or build via Visual Studio CMake tools).*

## 📖 Usage

### Start the Server
```bash
./server.exe
```
### Connect a Client
The server runs a simple plain-text protocol on `127.0.0.1:6379`. You can use `telnet` or `netcat`.
```bash
telnet 127.0.0.1 6379
```

#### Supported Commands
*   **SET:** `SET <key> <value>`
*   **SET EX:** `SET <key> <value> EX <seconds>` (Expiring Key)
*   **GET:** `GET <key>`
*   **DEL:** `DEL <key>`

## 🏗️ Internal Architecture
```text
Client Connection
      ↓
TCP Listener (Main Thread block)
      ↓
Push client FD to Bounded Task Queue
      ↓
Available Thread Pool Worker
      ↓
Parse Request (e.g., SET k1 v1 EX 60)
      ↓
Hash(k1) % 128 → Grants isolated Shard Mutex
      ↓
Insert to Hashmap / Update Doubly Linked List (LRU)
      ↓
Async write to WAL log file → Send Response to Client
```

## 🏗️ Future Technical Improvements
While optimized for scale, further enhancements could address latency and throughput bottlenecks:
- **Event-Driven I/O (`epoll`/`IOCP`):** Shifting from Thread-per-Connection mapping to non-blocking epoll queues to support 10k+ concurrent connections efficiently on fewer threads.
- **Cache-Line Alignment:** Padding the Shard structure to `alignas(64)` to strictly prevent CPU False Sharing when adjacent shards are locked simultaneously.
- **Timing Wheel / Min-Heap Expiration:** Upgrading the linear scan TTL background thread into an O(1) Timing Wheel buckets or O(log N) Min-Heap for highly scalable expiration processing.
