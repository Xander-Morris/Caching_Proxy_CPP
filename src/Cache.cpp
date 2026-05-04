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
    ttl_cv.notify_one();
}

void CacheSpace::Cache::clear() {
    std::unique_lock lock(mtx);

    cache_list.clear();
    cache_map.clear();
    url_hits_and_misses.clear();
    vary_specs.clear();
    
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

int64_t CacheSpace::Cache::GetCurrentSeconds() {
    auto now = std::chrono::system_clock::now();

    return std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();
}

bool CacheSpace::Cache::CheckHeapTop() {
    std::unique_lock lock(mtx);

    if (min_heap.empty()) return false;

    auto [url, expires_at] = min_heap.top();
    auto it = cache_map.find(url);

    if (it == cache_map.end() || it->second->second->expires_at != expires_at) {
        min_heap.pop();
        return true;
    }

    int64_t now = GetCurrentSeconds();

    if (now < expires_at) {
        return false;
    }

    cache_list.erase(it->second);
    cache_map.erase(it);
    min_heap.pop();

    return true;
}

void CacheSpace::Cache::LogEvent(const std::string &url, bool hit) {
    if (hit) {
        IncrementHits(url);
    } else {
        IncrementMisses(url);
    }
}

std::string CacheSpace::Cache::GetVarySpec(const std::string& path) const {
    std::shared_lock lock(mtx);
    auto it = vary_specs.find(path);
    return it != vary_specs.end() ? it->second : "";
}

void CacheSpace::Cache::SetVarySpec(const std::string& path, const std::string& vary) {
    std::unique_lock lock(mtx);
    vary_specs[path] = vary;
}