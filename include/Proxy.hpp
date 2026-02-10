#ifndef PROXY_HPP
#define PROXY_HPP

#include <string>
#include "Cache.hpp"
#include "httplib.h"

namespace ProxySpace {
    struct ProxyConfig {
        int port = 9090;
        std::string origin_url;
        int cache_size = 15;
        int ttl = 4; // in seconds
    }; 

    class Proxy {
    public:
        Proxy(ProxyConfig &config) : config(config), cache(config.cache_size, config.ttl), cli(config.origin_url.c_str()) {}
        void StartServer();
        void HandleRequest(const httplib::Request&, httplib::Response&);
        bool MatchesEndpoint(const std::string&, httplib::Response&);

    private:
        void TTLFunction();
        CacheSpace::Cache cache;
        ProxyConfig config;
        httplib::Client cli;
        httplib::Server svr;
    };
}

#endif 