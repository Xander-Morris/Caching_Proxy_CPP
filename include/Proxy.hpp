#ifndef PROXY_HPP
#define PROXY_HPP

#include <string>
#include <memory>
#include "Cache.hpp"
#include "httplib.h"

namespace ProxySpace {
    struct ProxyConfig {
        int port{9090};
        std::string origin_url;
        int cache_size{15};
        int ttl{4}; // in seconds
    }; 

    const std::array<std::string_view, 5> PROXY_FIELDS = {
        "port", "origin_url", "cache_size", "ttl",
    };
    using HttpClient = std::unique_ptr<httplib::SSLClient>;

    class Proxy {
    public:
        explicit Proxy(const ProxyConfig &config) : config(config), cache(config.cache_size, config.ttl) {
            cli = std::make_unique<httplib::SSLClient>(config.origin_url.c_str());
            cli->enable_server_certificate_verification(true); // ensures HTTPS works
            cli->set_keep_alive(false);
            cli->set_read_timeout(5, 0);
            cli->set_connection_timeout(5, 0);
        }
        void StartServer();
        void HandleRequest(const httplib::Request&, httplib::Response&);
        bool MatchesEndpoint(const std::string&, httplib::Response&);
        std::optional<int> ParseMaxAge(const std::string&);
        void LogMessage(const std::string&);

    private:
        void TTLFunction();
        CacheSpace::Cache cache;
        ProxyConfig config;
        ProxySpace::HttpClient cli;
        httplib::Server svr;
    };
}

#endif 