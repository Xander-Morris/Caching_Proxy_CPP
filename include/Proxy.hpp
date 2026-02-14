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

    struct ProxyConfig {
        int port{9090};
        std::string origin_url; // default
        int cache_size{15};
        int ttl{4}; // in seconds
        std::vector<ProxySpace::RouteConfig> routes; 
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

    static const std::array<std::string_view, 5> PROXY_FIELDS = {
        "port", "origin_url", "cache_size", "ttl",
    };
    using HttpClient = std::unique_ptr<httplib::SSLClient>;
    using CommandFunc = std::function<void(const httplib::Request&, httplib::Response&)>;

    class Proxy {
    public:
        explicit Proxy(const ProxyConfig &config) : config(config), cache(config.cache_size, config.ttl) {
            BuildClients();
            BuildEndpoints();
        }
        std::string MakeCacheKey(const httplib::Request&) const;
        void StartServer();
        void BuildClients();
        void BuildEndpoints();
        bool CheckCacheForResponse(const std::string &, httplib::Response &res);
        void HandleRequest(const httplib::Request&, httplib::Response&);
        bool MatchesEndpoint(const std::string&, const httplib::Request&, httplib::Response&);
        std::optional<int> ParseMaxAge(const std::string&);
        void LogMessage(const std::string&);
        std::string SelectOrigin(const std::string&) const;

    private:
        std::unordered_map<std::string, CommandFunc> endpoints;
        void TTLFunction();
        CacheSpace::Cache cache;
        ProxyConfig config;
        std::unordered_map<std::string, ProxySpace::HttpClient> clients;
        httplib::Server svr;
    };
}

#endif 