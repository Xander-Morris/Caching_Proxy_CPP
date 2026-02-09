#include "httplib.h"
#include "Cache.hpp"

struct ProxyConfig {
    int port = 9090;
    std::string origin_url;
    int cache_size = 15;
    int ttl = 600;
};

void LogCacheEvent(Cache &cache, const std::string &url, bool hit) {
    std::cout << "[" << (hit ? "HIT" : "MISS") << "] " << url << "\n";
    
    if (hit) {
        cache.IncrementHits();
    } else {
        cache.IncrementMisses();
    }
}

bool MatchesEndpoint(Cache &cache, const std::string &key, httplib::Response &res) {
    std::string response = "";

    if (key == "/stats") { 
        response = "HITS: " + std::to_string(cache.GetHits()) + ", MISSES: " + std::to_string(cache.GetMisses()) + "\n";
    }

    if (response != "") {
        res.set_content(response, "text/plain");
    }

    return response != "";
}

void HandleRequest(const httplib::Request &req, httplib::Response &res, Cache &cache, httplib::Client &cli) {
    std::string key = req.target; 

    if (key.empty()) { 
        key = "/"; 
    }

    if (MatchesEndpoint(cache, key, res)) {
        // Do not cache results from endpoint requests.
        return;
    }

    if (cache.HasUrl(key)) {
        const auto &cached = cache.get(key);
        res.status = cached.status;
        res.headers = cached.headers; 
        res.body = cached.body;

        LogCacheEvent(cache, key, true);

        return;
    }

    auto origin_res = cli.Get(req.target.c_str());  

    if (!origin_res) {
        std::string error_msg = "Proxy error: " + httplib::to_string(origin_res.error());
        res.status = 502;
        res.set_content(error_msg, "text/plain");

        return;
    }

    res.status = origin_res->status;
    res.headers = origin_res->headers;
    res.headers.insert({"X-Cache", "MISS"}); 
    res.body = origin_res->body;

    CachedResponse cached;
    cached.status = origin_res->status;
    cached.headers = origin_res->headers;
    cached.headers.insert({"X-Cache", "HIT"});
    cached.body = origin_res->body;
    cache.put(key, cached);

    LogCacheEvent(cache, key, false);
}

void StartServer(Cache &cache, const std::string &host, int port_number) {
    httplib::Client cli(host.c_str());
    httplib::Server svr;

    svr.Get("/.*", [&](const httplib::Request &req, httplib::Response &res) {
        HandleRequest(req, res, cache, cli);
    });

    bool started = svr.listen("localhost", port_number);

    if (!started) {
        std::cerr << "ERROR: Failed to bind to port " << port_number << ". It is likely being used by something else.\n";
    }
}

ProxyConfig ParseArgs(int argc, char *argv[]) {
    ProxyConfig config;
    int i = 1;

    while (i < argc) {
        std::string keyword = argv[i];

        if (keyword == "--port") {
            config.port = std::stoi(argv[++i]);
        }
        else if (keyword == "--origin-url") {
            config.origin_url = argv[++i];
        }
        else if (keyword == "--cache-size") {
            config.cache_size = std::stoi(argv[++i]);
        } 
        else if (keyword == "--ttl") {
            config.ttl = std::stoi(argv[++i]);
        }
        else if (keyword == "--clear-cache") {
            i++; 
        }

        i++;
    }

    return config;
}

int main(int argc, char *argv[])
{
    if (argc < 5) {
        std::cout << "Usage: proxy --port <port> --origin-url <url>\n";
        return 1;
    }

    ProxyConfig config = ParseArgs(argc, argv);
    Cache cache(config.cache_size, config.ttl);
    StartServer(cache, config.origin_url, config.port);
}