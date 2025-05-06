#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#include <random>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

std::atomic<int> successful_ops(0);
std::atomic<int> failed_ops(0);

void client_thread(int num_requests, const std::string& ip, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        failed_ops += num_requests;
        return;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        failed_ops += num_requests;
        WSACleanup();
        return;
    }

    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &hint.sin_addr);

    if (connect(clientSocket, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
        failed_ops += num_requests;
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    // Prepare RNG for workload
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> cmd_dist(0, 9); // 0-2 = SET, 3-8 = GET, 9 = DEL
    std::uniform_int_distribution<int> key_dist(0, 1000); // 1000 possible keys

    char buf[4096];

    for (int i = 0; i < num_requests; ++i) {
        int cmd_type = cmd_dist(rng);
        int key_id = key_dist(rng);
        std::string req;

        if (cmd_type <= 2) {
            req = "SET key" + std::to_string(key_id) + " val" + std::to_string(key_id) + "\r\n";
        } else if (cmd_type <= 8) {
            req = "GET key" + std::to_string(key_id) + "\r\n";
        } else {
            req = "DEL key" + std::to_string(key_id) + "\r\n";
        }

        if (send(clientSocket, req.c_str(), req.size(), 0) == SOCKET_ERROR) {
            failed_ops++;
            continue;
        }

        memset(buf, 0, 4096);
        int bytesReceived = recv(clientSocket, buf, 4096, 0);
        if (bytesReceived > 0) {
            successful_ops++;
        } else {
            failed_ops++;
        }
    }

    closesocket(clientSocket);
    WSACleanup();
}

int main() {
    int num_clients = 1000;
    int requests_per_client = 100;
    int total_requests = num_clients * requests_per_client;
    std::string ip = "127.0.0.1";
    int port = 6379;

    std::cout << "Starting KV Store Benchmark..." << std::endl;
    std::cout << "Concurrency: " << num_clients << " clients" << std::endl;
    std::cout << "Requests per client: " << requests_per_client << std::endl;
    std::cout << "Total Requests: " << total_requests << std::endl;

    std::vector<std::thread> clients;
    
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_clients; ++i) {
        clients.emplace_back(client_thread, requests_per_client, ip, port);
    }

    for (auto& t : clients) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    double ops_per_sec = successful_ops / duration.count();
    
    std::cout << "======================================" << std::endl;
    std::cout << "Finished in " << duration.count() << " seconds." << std::endl;
    std::cout << "Successful Ops: " << successful_ops << std::endl;
    std::cout << "Failed Ops: " << failed_ops << std::endl;
    std::cout << "Throughput: " << ops_per_sec << " ops/sec" << std::endl;
    std::cout << "======================================" << std::endl;

    return 0;
}
