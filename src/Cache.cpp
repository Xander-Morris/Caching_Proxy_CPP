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
    HeapPush({url, GetCurrentSeconds()});
    cache_map[url] = cache_list.begin();
}

void CacheSpace::Cache::clear() {
    std::unique_lock<std::shared_mutex> lock(mtx);
    cache_list.clear();
    cache_map.clear();
    
    // Since there is no "clear" method for the min heap.
    while (!min_heap.empty()) {
        min_heap.pop();
    }
}

// IncrementHits and IncrementMisses already lock, so none is needed here.
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

// Returns {"", 0} if nothing is in there.
CacheSpace::PQ_PAIR CacheSpace::Cache::HeapTop() {
    std::shared_lock<std::shared_mutex> lock(mtx);

    if (min_heap.size() <= 0) {
        return {"", 0};
    }

    return min_heap.top();
}

void CacheSpace::Cache::HeapPop() {
    std::lock_guard<std::shared_mutex> lock(mtx);

    if (min_heap.size() <= 0) return;

    std::cout << "Popping from the heap due to TTL issues.\n";
    auto p = min_heap.top();
    min_heap.pop();

    // I want to remove from the other containers as well.
    if (cache_map.contains(p.first)) {
        cache_map.erase(p.first);
    }

    auto MatchesP = [&](const CACHE_PAIR &cp) {
        return cp.first == p.first;
    };

    cache_list.remove_if(MatchesP);
    std::cout << *this;
}

void CacheSpace::Cache::LogEvent(const std::string &url, bool hit) {
    std::lock_guard<std::shared_mutex> lock(mtx);
    std::cout << "[" << (hit ? "HIT" : "MISS") << "] " << url << "\n";
    
    if (hit) {
        IncrementHits(url);
    } else {
        IncrementMisses(url);
    }
}