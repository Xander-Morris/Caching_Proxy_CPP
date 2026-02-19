#include "Cache.hpp"
#include <chrono>

std::shared_ptr<CacheSpace::CachedResponse> CacheSpace::Cache::get(const std::string& url) {
    std::unique_lock lock(mtx); 

    auto it = cache_map.find(url);

    if (it == cache_map.end()) {
        return nullptr;
    }

    cache_list.splice(cache_list.begin(), cache_list, it->second);

    return it->second->second; 
}

void CacheSpace::Cache::put(const std::string& url, const CachedResponse& cached) {
    std::unique_lock lock(mtx);

    auto it = cache_map.find(url);

    if (it != cache_map.end()) {
        it->second->second = std::make_shared<CachedResponse>(cached);
        cache_list.splice(cache_list.begin(), cache_list, it->second);
    } else {
        if (cache_list.size() >= capacity) {
            auto& last = cache_list.back();
            cache_map.erase(last.first);
            cache_list.pop_back();
        }

        cache_list.emplace_front(
            url,
            std::make_shared<CachedResponse>(cached)
        );

        cache_map[url] = cache_list.begin();
    }

    min_heap.push({url, cached.expires_at});
}

void CacheSpace::Cache::clear() {
    std::unique_lock lock(mtx);

    cache_list.clear();
    cache_map.clear();
    url_hits_and_misses.clear();
    
    // Since there is no "clear" method for the min heap.
    while (!min_heap.empty()) {
        min_heap.pop();
    }
}

void CacheSpace::Cache::IncrementURLHitsOrMisses(const std::string& key, bool is_hit) {
    std::unique_lock lock(mtx);

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
    std::unique_lock lock(mtx);

    while (!min_heap.empty()) {
        auto [url, expires_at] = min_heap.top();
        auto it = cache_map.find(url);

        if (it == cache_map.end() || it->second->second->expires_at != expires_at) {
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