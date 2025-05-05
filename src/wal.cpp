#include "wal.hpp"
#include <iostream>

WriteAheadLog::WriteAheadLog(const std::string& filepath) : filename(filepath) {
    // Open in append mode
    log_file.open(filename, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "Failed to open WAL file: " << filename << std::endl;
    }
}

WriteAheadLog::~WriteAheadLog() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

void WriteAheadLog::append(const std::string& command_line) {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (log_file.is_open()) {
        log_file << command_line << "\n";
        log_file.flush(); // Ensure it hits the disk
    }
}

void WriteAheadLog::replay(KVStore& store) {
    std::ifstream in_file(filename);
    if (!in_file.is_open()) {
        return; // File might not exist yet, which is fine
    }

    std::string command;
    std::string key;
    std::string value;

    // A very simple replay implementation
    while (in_file >> command >> key) {
        if (command == "SET") {
            in_file >> value;
            // Check if there is an EX parameter
            std::string next_token;
            if (in_file >> next_token) {
                if (next_token == "EX") {
                    int ttl;
                    in_file >> ttl;
                    store.set_ex(key, value, ttl);
                } else {
                    store.set(key, value);
                    // Just in case it was a valid command on the same line,
                    // we'd need better parsing, but we assume simple format for now.
                }
            } else {
                store.set(key, value);
            }
        } else if (command == "DEL") {
            store.del(key);
        }
    }
}
