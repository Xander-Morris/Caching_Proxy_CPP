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
        std::cout << "Popped off from the LRU cache!\n";
    }

    cache_list.push_front({url, cached});
    min_heap.push(p);
    cache_map[url] = cache_list.begin();
}

// This is not currently called anywhere, but I have it here just in case. 
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

// No lock is needed here.
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

void CacheSpace::Cache::Evict(const std::string &key) {
    if (min_heap.size() <= 0) return;

    std::lock_guard<std::shared_mutex> lock(mtx);
    auto p = min_heap.top();

    if (p.first == key) {
        min_heap.pop();
    }

    if (cache_map.contains(p.first)) {
        cache_map.erase(p.first);
    }

    auto MatchesP = [&](const CACHE_PAIR &cp) {
        return cp.first == p.first;
    };

    cache_list.remove_if(MatchesP);
    std::cout << *this;
}

bool CacheSpace::Cache::CheckHeapTop() {
    if (min_heap.size() <= 0) return false;
    if (GetCurrentSeconds() < min_heap.top().second) return false;

    Evict(min_heap.top().first);

    return true;
}

void CacheSpace::Cache::LogEvent(const std::string &url, bool hit) {    
    if (hit) {
        IncrementHits(url);
    } else {
        IncrementMisses(url);
    }
}