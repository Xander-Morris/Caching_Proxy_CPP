#ifndef CACHE_HPP
#define CACHE_HPP

#include <unordered_map>
#include <queue>
#include <list>
#include <mutex>
#include "httplib.h"

namespace CacheSpace {
    struct CachedResponse
    {
        int status;
        httplib::Headers headers;
        std::string body;
    };

    using CACHE_PAIR = std::pair<std::string, CachedResponse>;
    using PQ_PAIR = std::pair<std::string, int>;

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
        PQ_PAIR HeapTop(); // Returns {"", 0} if nothing is in there.
        void HeapPush(PQ_PAIR);
        void HeapPop();
        friend std::ostream& operator<<(std::ostream& os, const Cache& cache);
        void LogEvent(const std::string&, bool);

    private:
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

    inline std::ostream& operator<<(std::ostream& os, const CacheSpace::Cache& cache) {
        os << "CACHE DATA\n"
        << "Hits: " << cache.hits
        << ", Misses: " << cache.misses << "\n"
        << "Capacity: " << cache.capacity << "\n"
        << "TTL Seconds: " << cache.TTLSeconds << "\n"
        << "Size of cache: " << cache.cache_list.size() << "\n";

        return os;
    }
};

#endif 