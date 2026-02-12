#ifndef PROXY_HPP
#define PROXY_HPP

#include <string>
#include <memory>
#include "Cache.hpp"
#include "httplib.h"

namespace ProxySpace {
    struct RouteConfig {
        std::string prefix;  
        std::string origin; 
    };

    static const std::unordered_set<std::string> hop_by_hop = {
        "connection",
        "keep-alive",
        "proxy-authenticate",
        "proxy-authorization",
        "te",
        "trailer",
        "transfer-encoding",
        "upgrade",
        "content-length"
    };

    struct ProxyConfig {
        int port{9090};
        std::string origin_url; // default
        int cache_size{15};
        int ttl{4}; // in seconds
        std::vector<ProxySpace::RouteConfig> routes; 
    }; 

    const std::array<std::string_view, 5> PROXY_FIELDS = {
        "port", "origin_url", "cache_size", "ttl",
    };
    using HttpClient = std::unique_ptr<httplib::SSLClient>;

    class Proxy {
    public:
        explicit Proxy(const ProxyConfig &config) : config(config), cache(config.cache_size, config.ttl) {
            const auto create_client = [&](const std::string &origin) {
                HttpClient client = std::make_unique<httplib::SSLClient>(origin.c_str());
                client->enable_server_certificate_verification(true); // ensures HTTPS works
                client->set_keep_alive(true);
                client->set_read_timeout(5, 0);
                client->set_connection_timeout(5, 0);

                return client;
            };

            clients[config.origin_url] = create_client(config.origin_url);
            
            for (const auto& route : config.routes) {
                clients[route.origin] = create_client(route.origin);
                std::cout << "Created client for route prefix: " << route.prefix << ", origin: " << route.origin << "\n";
            }
        }
        std::string MakeCacheKey(const httplib::Request&) const;
        void StartServer();
        bool CheckCacheForResponse(const std::string &, httplib::Response &res);
        void HandleRequest(const httplib::Request&, httplib::Response&);
        bool MatchesEndpoint(const std::string&, httplib::Response&);
        std::optional<int> ParseMaxAge(const std::string&);
        void LogMessage(const std::string&);
        std::string SelectOrigin(const std::string&) const;

    private:
        void TTLFunction();
        CacheSpace::Cache cache;
        ProxyConfig config;
        std::unordered_map<std::string, ProxySpace::HttpClient> clients;
        httplib::Server svr;
    };
}

#endif 