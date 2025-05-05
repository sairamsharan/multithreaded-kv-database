#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include "kv_store.hpp"

class WriteAheadLog {
private:
    std::mutex file_mutex;
    std::ofstream log_file;
    std::string filename;

public:
    explicit WriteAheadLog(const std::string& filepath);
    ~WriteAheadLog();

    // Prevent copy
    WriteAheadLog(const WriteAheadLog&) = delete;
    WriteAheadLog& operator=(const WriteAheadLog&) = delete;

    // Append to log synchronously
    void append(const std::string& command_line);

    // Replay on startup
    void replay(KVStore& store);
};
