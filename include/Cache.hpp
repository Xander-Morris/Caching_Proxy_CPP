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
    using HITS_AND_MISSES_PAIR = std::pair<long long, long long>;

    class Cache {
    public:
        Cache(int capacity, int ttl_seconds) : capacity(capacity), ttl_seconds(ttl_seconds) {}
        bool HasUrl(const std::string &);
        CachedResponse get(const std::string &);
        void put(const std::string &, const CachedResponse &);
        void IncrementURLHitsOrMisses(const std::string&, bool);
        void IncrementHits(const std::string&);
        void IncrementMisses(const std::string&);
        int GetHits();
        int GetMisses();
        const std::unordered_map<std::string, CacheSpace::HITS_AND_MISSES_PAIR> GetURLHitsAndMisses() const;
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
        std::unordered_map<std::string, std::list<CacheSpace::CACHE_PAIR>::iterator> cache_map;
        std::unordered_map<std::string, CacheSpace::HITS_AND_MISSES_PAIR> url_hits_and_misses; 
        std::priority_queue<PQ_PAIR, std::vector<PQ_PAIR>, ComparePQPairs> min_heap;
        std::mutex mtx;
        int hits = 0;
        int misses = 0;
        int capacity;
        int ttl_seconds;
    };

    inline std::ostream& operator<<(std::ostream& os, const CacheSpace::Cache& cache) {
        os << "CACHE DATA\n"
        << "Hits: " << cache.hits
        << ", Misses: " << cache.misses << "\n"
        << "Capacity: " << cache.capacity << "\n"
        << "TTL Seconds: " << cache.ttl_seconds << "\n"
        << "Size of cache: " << cache.cache_list.size() << "\n";

        return os;
    }
};

#endif 