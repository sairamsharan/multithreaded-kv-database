#pragma once

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <vector>
#include <list>
#include <chrono>

// For TTL
using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

struct ValueData {
    std::string value;
    bool has_ttl = false;
    TimePoint expiration;
};

class KVStore {
private:
    static constexpr size_t NUM_SHARDS = 128; // Lock striping size
    
    // Each shard has its own unordered_map and shared_mutex
    struct Shard {
        std::unordered_map<std::string, std::pair<ValueData, std::list<std::string>::iterator>> map;
        std::unique_ptr<std::shared_mutex> mutex;
        
        Shard() : mutex(std::make_unique<std::shared_mutex>()) {}
    };

    std::vector<Shard> shards;

    // LRU specific: Global doubly-linked list ordering accesses (MRU at front, LRU at back)
    std::list<std::string> lru_list;
    std::mutex lru_mutex; // Global lock for LRU list operations
    size_t capacity;
    size_t current_size;

    size_t get_shard_index(const std::string& key) const;
    void evict_if_needed();
    void update_lru(const std::string& key);

public:
    explicit KVStore(size_t max_capacity = 100000);
    ~KVStore() = default;

    // Prohibit copy semantic
    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;

    // Core operations
    std::string set(const std::string& key, const std::string& value);
    std::string set_ex(const std::string& key, const std::string& value, int ttl_seconds);
    std::string get(const std::string& key);
    std::string del(const std::string& key);

    // TTL cleanup
    void remove_expired_keys();
};
