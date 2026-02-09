#ifndef CACHE_HPP
#define CACHE_HPP

#include <unordered_map>
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
    Cache(int capacity) : capacity(capacity) {}
    bool HasUrl(const std::string &);
    CachedResponse get(const std::string &);
    void put(const std::string &, const CachedResponse &);
    void clear();

private:
    using CACHE_PAIR = std::pair<std::string, CachedResponse>;
    int capacity;
    std::list<CACHE_PAIR> cache_list;
    std::unordered_map<std::string, std::list<CACHE_PAIR>::iterator> cache_map;
    std::mutex mtx;
};

#endif 