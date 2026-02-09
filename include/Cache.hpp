#ifndef CACHE_HPP
#define CACHE_HPP

#include <unordered_map>
#include <queue>
#include <list>
#include <mutex>
#include "httplib.h"

struct CachedResponse
{
    int status;
    httplib::Headers headers;
    std::string body;
};

class Cache {
public:
    Cache(int capacity, int TTLSeconds) : capacity(capacity), TTLSeconds(TTLSeconds) {}
    bool HasUrl(const std::string &);
    CachedResponse get(const std::string &);
    void put(const std::string &, const CachedResponse &);
    void IncrementHits();
    void IncrementMisses();
    int GetHits();
    int GetMisses();
    void clear();
    int GetCurrentSeconds();

private:
    using CACHE_PAIR = std::pair<std::string, CachedResponse>;
    using PQ_PAIR = std::pair<std::string, int>;

    struct ComparePQPairs {
        bool operator()(const PQ_PAIR& a, const PQ_PAIR& b) const {
            // We want the earliest entry times on the top of the min heap.
            return a.second > b.second;
        }
    };
    
    std::list<CACHE_PAIR> cache_list; 
    std::unordered_map<std::string, std::list<CACHE_PAIR>::iterator> cache_map;
    std::priority_queue<PQ_PAIR, std::vector<PQ_PAIR>, ComparePQPairs> min_heap;
    std::mutex mtx;
    int hits = 0;
    int misses = 0;
    int capacity;
    int TTLSeconds;
};

#endif 