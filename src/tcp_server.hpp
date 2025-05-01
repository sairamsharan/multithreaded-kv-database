#pragma once

#include <vector>
#include <string>

// Need to define WIN32_LEAN_AND_MEAN to avoid conflicts
// between WinSock2.h and Windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include "thread_pool.hpp"
#include "kv_store.hpp"
#include "wal.hpp"

class TCPServer {
private:
    std::string ip_address;
    int port;
    int buffer_size;
    SOCKET listening_socket;
    
    ThreadPool& pool;
    KVStore& store;
    WriteAheadLog& wal;

    bool init_winsock();
    SOCKET create_socket();

    // Client handling function run by worker threads
    void handle_client(SOCKET client_socket);

    // Command parser
    std::string parse_and_execute(const std::string& request);

public:
    TCPServer(const std::string& ip_address, int port, int buffer_size, 
              ThreadPool& pool, KVStore& store, WriteAheadLog& wal);
    ~TCPServer();

    // Prevent copy
    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;

    // Start accepting clients
    void start();
};
