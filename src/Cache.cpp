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
    HeapPush({url, GetCurrentSeconds()});
    cache_map[url] = cache_list.begin();
}

void Cache::clear() {
    std::lock_guard<std::mutex> lock(mtx);
    cache_list.clear();
    cache_map.clear();
    
    // Since there is no "clear" method for the min heap.
    while (!min_heap.empty()) {
        min_heap.pop();
    }
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

// Returns {"", 0} if nothing is in there.
PQ_PAIR Cache::HeapTop() {
    if (min_heap.size() <= 0) {
        return {"", 0};
    }

    return min_heap.top();
}

void Cache::HeapPush(PQ_PAIR p) {
    min_heap.push(p);
}

void Cache::HeapPop() {
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

    std::cout << "New container sizes after popping from the heap:\nMin heap size: " 
        << min_heap.size() << ", Cache map size: " << cache_map.size() << ", Cache list size: " << cache_list.size() << "\n";
}