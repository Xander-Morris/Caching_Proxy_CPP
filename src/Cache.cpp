#include "Cache.hpp"
#include <chrono>

bool Cache::HasUrl(const std::string &url) {
    std::lock_guard<std::mutex> lock(mtx);
    return cache_map.find(url) != cache_map.end();
}

CachedResponse Cache::get(const std::string &url) {
    std::lock_guard<std::mutex> lock(mtx);

    if (cache_map.find(url) == cache_map.end()) {
        return CachedResponse{};
    }
    
    cache_list.splice(cache_list.begin(), cache_list, cache_map[url]);

    return cache_map[url]->second;
}

void Cache::put(const std::string &url, const CachedResponse &cached) {
    std::lock_guard<std::mutex> lock(mtx);

    if (cache_map.find(url) != cache_map.end()) {
        cache_map[url]->second = cached;
        cache_list.splice(cache_list.begin(), cache_list, cache_map[url]);

        return;
    }

    if (cache_list.size() >= capacity) {
        std::string last_key = cache_list.back().first;
        cache_map.erase(last_key);
        cache_list.pop_back();
        std::cout << "Popped off from the LRU cache!\n";
    }

    cache_list.push_front({url, cached});
    cache_map[url] = cache_list.begin();
}

void Cache::clear() {
    std::lock_guard<std::mutex> lock(mtx);
    cache_list.clear();
    cache_map.clear();
}

void Cache::IncrementHits() {
    std::lock_guard<std::mutex> lock(mtx);
    hits += 1;
}

void Cache::IncrementMisses() {
    std::lock_guard<std::mutex> lock(mtx);
    misses += 1;
}

int Cache::GetHits() {
    return hits;
}

int Cache::GetMisses() {
    return misses;
}

int Cache::GetCurrentSeconds() {
    auto now = std::chrono::system_clock::now();

    auto seconds_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();

    return seconds_since_epoch;
}