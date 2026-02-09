#include "httplib.h"
#include "Cache.hpp"
#include <thread>
#include <chrono>
#include <functional> 

struct ProxyConfig {
    int port = 9090;
    std::string origin_url;
    int cache_size = 15;
    int ttl = 60; // in seconds
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

void TTLFunction(Cache &cache, const ProxyConfig &config) {
    const int interval = config.ttl * 1000 * 0.25;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        const int current_seconds = cache.GetCurrentSeconds();
        std::cout << "Doing a TTL check.";
        
        while (true) {
            // Returns {"", 0} if nothing is in there.
            auto top = cache.HeapTop();
            std::cout << top.first << "\n";

            if (top.first == "" || current_seconds - top.second < config.ttl) {
                break;
            }

            cache.HeapPop();
        }
    }
}

void StartServer(Cache &cache, const ProxyConfig &config) {
    std::thread ttl_thread(TTLFunction, std::ref(cache), std::ref(config));
    httplib::Client cli(config.origin_url.c_str());
    httplib::Server svr;

    svr.Get("/.*", [&](const httplib::Request &req, httplib::Response &res) {
        HandleRequest(req, res, cache, cli);
    });

    bool started = svr.listen("localhost", config.port);

    if (!started) {
        std::cerr << "ERROR: Failed to bind to port " << config.port << ". It is likely being used by something else.\n";
    }
}

ProxyConfig ParseArgs(int argc, char *argv[]) {
    ProxyConfig config;
    int i = 1;

    using CommandFunc = std::function<void()>;
    std::unordered_map<std::string, CommandFunc> commands = {
        {"--port", [&]() {
            config.port = std::stoi(argv[++i]);
        }},
        {"--origin-url", [&]() {
            config.origin_url = argv[++i];
        }},
        {"--cache-size", [&]() {
            config.cache_size = std::stoi(argv[++i]);
        }},
        {"--ttl", [&]() {
            config.ttl = std::stoi(argv[++i]);
        }},
    };

    while (i < argc) {
        std::string command = argv[i];

        if (!commands.contains(command)) {
            throw new std::runtime_error("Invalid command: " + command + "!");
        }

        commands[command]();
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
    StartServer(cache, config);
}