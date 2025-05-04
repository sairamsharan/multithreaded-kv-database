#include "kv_store.hpp"
#include <iterator>

KVStore::KVStore(size_t max_capacity) : capacity(max_capacity), current_size(0) {
    shards.resize(NUM_SHARDS);
}

size_t KVStore::get_shard_index(const std::string& key) const {
    return std::hash<std::string>{}(key) % NUM_SHARDS;
}

void KVStore::update_lru(const std::string& key) {
    std::lock_guard<std::mutex> lock(lru_mutex);
    lru_list.remove(key); // Remove if exists
    lru_list.push_front(key); // Add to front (MRU)
}

void KVStore::evict_if_needed() {
    std::string evict_key;
    {
        std::lock_guard<std::mutex> lock(lru_mutex);
        if (current_size < capacity || lru_list.empty()) return;
        
        // The last element is LRU
        evict_key = lru_list.back();
        lru_list.pop_back();
    }
    
    // Now remove from the map
    size_t s_idx = get_shard_index(evict_key);
    std::unique_lock<std::shared_mutex> ulock(*(shards[s_idx].mutex));
    
    if (shards[s_idx].map.erase(evict_key)) {
        --current_size;
    }
}

std::string KVStore::set(const std::string& key, const std::string& value) {
    auto now = std::chrono::system_clock::now();
    size_t s_idx = get_shard_index(key);
    
    evict_if_needed();
    update_lru(key);

    std::unique_lock<std::shared_mutex> lock(*(shards[s_idx].mutex));
    auto& shard_map = shards[s_idx].map;
    
    ValueData data;
    data.value = value;
    data.has_ttl = false;
    
    // In LRU list, we technically should store the iterator, but for simplicity we rely on string
    if (shard_map.find(key) == shard_map.end()) {
        ++current_size;
    }
    shard_map[key] = {data, lru_list.begin()};
    
    return "OK";
}

std::string KVStore::set_ex(const std::string& key, const std::string& value, int ttl_seconds) {
    auto now = std::chrono::system_clock::now();
    size_t s_idx = get_shard_index(key);
    
    evict_if_needed();
    update_lru(key);

    std::unique_lock<std::shared_mutex> lock(*(shards[s_idx].mutex));
    auto& shard_map = shards[s_idx].map;
    
    ValueData data;
    data.value = value;
    data.has_ttl = true;
    data.expiration = now + std::chrono::seconds(ttl_seconds);
    
    if (shard_map.find(key) == shard_map.end()) {
        ++current_size;
    }
    shard_map[key] = {data, lru_list.begin()};

    return "OK";
}

std::string KVStore::get(const std::string& key) {
    size_t s_idx = get_shard_index(key);
    
    // Update LRU on read
    update_lru(key);

    std::shared_lock<std::shared_mutex> lock(*(shards[s_idx].mutex));
    auto it = shards[s_idx].map.find(key);
    
    if (it != shards[s_idx].map.end()) {
        // Check TTL
        if (it->second.first.has_ttl && std::chrono::system_clock::now() > it->second.first.expiration) {
            // Lazy expiration: technically we should delete it here, but to avoid 
            // lock escalation from shared to unique, we just return NOT FOUND 
            // and let the cleaner thread handle actual deletion
            return "(nil)";
        }
        return it->second.first.value;
    }
    return "(nil)";
}

std::string KVStore::del(const std::string& key) {
    size_t s_idx = get_shard_index(key);
    
    {
        std::lock_guard<std::mutex> lru_lock(lru_mutex);
        lru_list.remove(key);
    }

    std::unique_lock<std::shared_mutex> lock(*(shards[s_idx].mutex));
    if (shards[s_idx].map.erase(key)) {
        --current_size;
        return "1"; // Deleted 1 key
    }
    return "0";
}

// Meant to be run in a background thread periodically
void KVStore::remove_expired_keys() {
    auto now = std::chrono::system_clock::now();
    for(size_t i = 0; i < NUM_SHARDS; ++i) {
        std::unique_lock<std::shared_mutex> lock(*(shards[i].mutex));
        for(auto it = shards[i].map.begin(); it != shards[i].map.end(); ) {
            if (it->second.first.has_ttl && now > it->second.first.expiration) {
                // Delete from LRU list too
                {
                    std::lock_guard<std::mutex> lru_lock(lru_mutex);
                    lru_list.remove(it->first);
                }
                it = shards[i].map.erase(it);
                --current_size;
            } else {
                ++it;
            }
        }
    }
}
