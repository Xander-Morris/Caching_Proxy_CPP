#ifndef PROXY_HPP
#define PROXY_HPP

#include <string>
#include <memory>
#include "Cache.hpp"
#include "httplib.h"

namespace ProxySpace {
    struct ProxyConfig {
        int port = 9090;
        std::string origin_url;
        int cache_size = 15;
        int ttl = 4; // in seconds
    }; 

    using HttpClient = std::unique_ptr<httplib::SSLClient>;

    class Proxy {
    public:
        Proxy(ProxyConfig &config) : config(config), cache(config.cache_size, config.ttl) {
            cli = std::make_unique<httplib::SSLClient>(config.origin_url.c_str());
            cli->enable_server_certificate_verification(true); // ensures HTTPS works
        }
        void StartServer();
        void HandleRequest(const httplib::Request&, httplib::Response&);
        bool MatchesEndpoint(const std::string&, httplib::Response&);

    private:
        void TTLFunction();
        CacheSpace::Cache cache;
        ProxyConfig config;
        ProxySpace::HttpClient cli;
        httplib::Server svr;
    };
}

#endif 