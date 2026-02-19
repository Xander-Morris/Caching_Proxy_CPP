#ifndef CACHE_HPP
#define CACHE_HPP

#include <unordered_map>
#include <queue>
#include <list>
#include <shared_mutex>
#include <atomic>
#include "httplib.h"

namespace CacheSpace {
    struct CachedResponse
    {
        int status;
        int expires_at;
        httplib::Headers headers;
        std::string body;
    };

    using CACHE_PAIR = std::pair<std::string, std::shared_ptr<CachedResponse>>;
    using PQ_PAIR = std::pair<std::string, int>; // url, expire time
    using HITS_AND_MISSES_PAIR = std::pair<long long, long long>;

    class Cache {
    public:
        Cache(int capacity, int ttl_seconds) : capacity(capacity), ttl_seconds(ttl_seconds) {}

        std::shared_ptr<CachedResponse> get(const std::string &);
        void put(const std::string &, const CachedResponse &);

        void IncrementURLHitsOrMisses(const std::string&, bool);
        void IncrementHits(const std::string& key) { 
            hits.fetch_add(1, std::memory_order_relaxed); 
            IncrementURLHitsOrMisses(key, true);
        }
        void IncrementMisses(const std::string& key) { 
            misses.fetch_add(1, std::memory_order_relaxed); 
            IncrementURLHitsOrMisses(key, false);
        }
        void IncrementCompliantMisses() {
            compliant_misses.fetch_add(1, std::memory_order_relaxed);
        }
        int GetHits() const { return hits.load(std::memory_order_relaxed); }
        int GetMisses() const { return misses.load(std::memory_order_relaxed); }
        int GetCompliantMisses() const { return compliant_misses.load(std::memory_order_relaxed); }

        const std::unordered_map<std::string, CacheSpace::HITS_AND_MISSES_PAIR> GetURLHitsAndMisses() const {
            std::shared_lock lock(mtx);
            return url_hits_and_misses;
        }
        void clear();
        int GetCurrentSeconds();
        bool CheckHeapTop();
        void LogEvent(const std::string&, bool);

        friend std::ostream& operator<<(std::ostream& os, const Cache& cache);

    private:
        struct ComparePQPairs {
            bool operator()(const PQ_PAIR& a, const PQ_PAIR& b) const {
                // We want the earliest expire times on the top of the min heap.
                return a.second > b.second;
            }
        };
        
        std::list<CACHE_PAIR> cache_list; 
        std::unordered_map<std::string, std::list<CacheSpace::CACHE_PAIR>::iterator> cache_map;
        std::unordered_map<std::string, CacheSpace::HITS_AND_MISSES_PAIR> url_hits_and_misses; 
        std::priority_queue<PQ_PAIR, std::vector<PQ_PAIR>, ComparePQPairs> min_heap;
        mutable std::shared_mutex mtx;
        std::atomic<int> hits{0};
        std::atomic<int> misses{0};
        std::atomic<int> compliant_misses{0};
        int capacity;
        int ttl_seconds;
    };

    // Declared outside of the class.
    inline std::ostream& operator<<(std::ostream& os, const CacheSpace::Cache& cache) {
        os << "CACHE DATA\n"
        << "Hits: " << cache.hits
        << ", Misses: " << cache.misses << "\n"
        << ", Compliant Misses: " << cache.compliant_misses << "\n"
        << "Capacity: " << cache.capacity << "\n"
        << "TTL Seconds: " << cache.ttl_seconds << "\n"
        << "Size of cache: " << cache.cache_list.size() << "\n";

        return os;
    }
};

#endif 