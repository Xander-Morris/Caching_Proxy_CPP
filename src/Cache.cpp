#include "Cache.hpp"

bool Cache::HasUrl(const std::string &url) {
    return cache_map.find(url) != cache_map.end();
}

CachedResponse Cache::get(const std::string &url) {
    if (cache_map.find(url) == cache_map.end()) return CachedResponse{};
    
    cache_list.splice(cache_list.begin(), cache_list, cache_map[url]);

    return cache_map[url]->second;
}

void Cache::put(const std::string &url, const CachedResponse &cached) {
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
    cache_list.clear();
    cache_map.clear();
}