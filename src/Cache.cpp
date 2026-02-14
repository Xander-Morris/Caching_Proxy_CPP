#include "Cache.hpp"
#include <chrono>

CacheSpace::CachedResponse CacheSpace::Cache::get(const std::string &url) {
    std::shared_lock<std::shared_mutex> lock(mtx);

    if (cache_map.find(url) == cache_map.end()) {
        return CachedResponse{};
    }
    
    cache_list.splice(cache_list.begin(), cache_list, cache_map[url]);

    return cache_map[url]->second;
}

void CacheSpace::Cache::put(const std::string &url, const CachedResponse &cached) {
    std::unique_lock<std::shared_mutex> lock(mtx);

    if (cache_map.find(url) != cache_map.end()) {
        cache_map[url]->second = cached;
        cache_list.splice(cache_list.begin(), cache_list, cache_map[url]);

        return;
    }

    if (cache_list.size() >= capacity) {
        std::string last_key = cache_list.back().first;
        cache_map.erase(last_key);
        cache_list.pop_back();
    }

    cache_list.push_front({url, cached});
    min_heap.push({url, cached.expires_at});
    cache_map[url] = cache_list.begin();
}

void CacheSpace::Cache::clear() {
    std::unique_lock<std::shared_mutex> lock(mtx);
    cache_list.clear();
    cache_map.clear();
    url_hits_and_misses.clear();
    
    // Since there is no "clear" method for the min heap.
    while (!min_heap.empty()) {
        min_heap.pop();
    }
}

void CacheSpace::Cache::IncrementURLHitsOrMisses(const std::string& key, bool is_hit) {
    std::unique_lock<std::shared_mutex> lock(mtx);

    if (!url_hits_and_misses.contains(key)) {
        url_hits_and_misses[key] = {0, 0};
    }

    url_hits_and_misses[key].first += is_hit ? 1 : 0;
    url_hits_and_misses[key].second += is_hit ? 0 : 1;
}

int CacheSpace::Cache::GetCurrentSeconds() {
    auto now = std::chrono::system_clock::now();

    auto seconds_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();

    return seconds_since_epoch;
}

bool CacheSpace::Cache::CheckHeapTop() {
    if (min_heap.size() <= 0) return false;
    if (GetCurrentSeconds() < min_heap.top().second) return false;

    std::unique_lock lock(mtx);

    while (!min_heap.empty()) {
        auto [url, expires_at] = min_heap.top();

        auto it = cache_map.find(url);

        if (it == cache_map.end() || it->second->second.expires_at != expires_at) {
            min_heap.pop();
            continue;
        }

        int now = GetCurrentSeconds();
        if (now < expires_at) {
            return false;
        }

        cache_list.erase(it->second);   
        cache_map.erase(it);       
        min_heap.pop();

        return true;
    }

    return false;
}

void CacheSpace::Cache::LogEvent(const std::string &url, bool hit) {    
    if (hit) {
        IncrementHits(url);
    } else {
        IncrementMisses(url);
    }
}