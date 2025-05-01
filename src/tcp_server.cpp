#include "tcp_server.hpp"
#include <iostream>
#include <sstream>

TCPServer::TCPServer(const std::string& ip, int p, int buffer, 
                     ThreadPool& tp, KVStore& kvs, WriteAheadLog& walog)
    : ip_address(ip), port(p), buffer_size(buffer), 
      pool(tp), store(kvs), wal(walog)
{
    if (init_winsock()) {
        listening_socket = create_socket();
    } else {
        listening_socket = INVALID_SOCKET;
    }
}

TCPServer::~TCPServer() {
    if (listening_socket != INVALID_SOCKET) {
        closesocket(listening_socket);
    }
    WSACleanup();
}

bool TCPServer::init_winsock() {
    WSADATA wsaData;
    int wsResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsResult != 0) {
        std::cerr << "Can't initialize winsock! Quitting" << std::endl;
        return false;
    }
    return true;
}

SOCKET TCPServer::create_socket() {
    SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
    if (listening == INVALID_SOCKET) {
        std::cerr << "Can't create a socket! Quitting" << std::endl;
        return INVALID_SOCKET;
    }

    // Bind the ip address and port to a socket
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(port);
    inet_pton(AF_INET, ip_address.c_str(), &hint.sin_addr);

    if (bind(listening, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
        std::cerr << "Can't bind to IP/port! Error #" << WSAGetLastError() << " Quitting" << std::endl;
        closesocket(listening);
        return INVALID_SOCKET;
    }

    // Tell Winsock the socket is for listening 
    if (listen(listening, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Can't listen on socket! Error #" << WSAGetLastError() << " Quitting" << std::endl;
        closesocket(listening);
        return INVALID_SOCKET;
    }

    return listening;
}

void TCPServer::start() {
    if (listening_socket == INVALID_SOCKET) return;

    std::cout << "Server is listening on " << ip_address << ":" << port << std::endl;

    while (true) {
        sockaddr_in client;
        int clientSize = sizeof(client);

        // Accept a client connection
        SOCKET clientSocket = accept(listening_socket, (sockaddr*)&client, &clientSize);

        if (clientSocket != INVALID_SOCKET) {
            // Push client handling onto the ThreadPool instead of blocking the accept loop
            pool.enqueue([this, clientSocket]() {
                this->handle_client(clientSocket);
            });
        }
    }
}

std::string TCPServer::parse_and_execute(const std::string& request) {
    std::istringstream iss(request);
    std::string command, key, value, ext_cmd;
    int ttl;
    
    if (!(iss >> command)) return "-ERR Unknown command\r\n";

    if (command == "SET") {
        if (!(iss >> key >> value)) return "-ERR Wrong number of arguments for SET\r\n";
        
        if (iss >> ext_cmd && ext_cmd == "EX" && iss >> ttl) {
            wal.append("SET " + key + " " + value + " EX " + std::to_string(ttl));
            return "+" + store.set_ex(key, value, ttl) + "\r\n";
        } else {
            wal.append("SET " + key + " " + value);
            return "+" + store.set(key, value) + "\r\n";
        }
    } 
    else if (command == "GET") {
        if (!(iss >> key)) return "-ERR Wrong number of arguments for GET\r\n";
        std::string res = store.get(key);
        if (res == "(nil)") return "$-1\r\n"; // Redis nil response format
        return "+" + res + "\r\n";
    } 
    else if (command == "DEL") {
        if (!(iss >> key)) return "-ERR Wrong number of arguments for DEL\r\n";
        std::string res = store.del(key);
        wal.append("DEL " + key);
        return ":" + res + "\r\n";
    }

    return "-ERR Unknown command\r\n";
}

void TCPServer::handle_client(SOCKET clientSocket) {
    char* buf = new char[buffer_size];
    
    // Welcome message optional, skip for benchmarks
    
    while (true) {
        ZeroMemory(buf, buffer_size);
        int bytesReceived = recv(clientSocket, buf, buffer_size, 0);

        if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "Error in recv()! Quitting client connection." << std::endl;
            break;
        }

        if (bytesReceived == 0) {
            // Client gracefully closed connection
            break;
        }

        // Parse command from string
        std::string request(buf, bytesReceived);

        // Often network transmits will have \n or \r\n, so our parser should handle words.
        std::string response = parse_and_execute(request);
        
        // Send response back
        send(clientSocket, response.c_str(), response.size() + 1, 0);
    }

    // Cleanup
    closesocket(clientSocket);
    delete[] buf;
}
