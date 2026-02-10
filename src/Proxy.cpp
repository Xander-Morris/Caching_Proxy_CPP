#include "Proxy.hpp"

bool ProxySpace::Proxy::MatchesEndpoint(const std::string &key, httplib::Response &res) {
    using CommandFunc = std::function<void()>;
    std::unordered_map<std::string, CommandFunc> commands = {
        {"/stats", [&]() {
            auto hits_and_misses = cache.GetURLHitsAndMisses();
            std::string per_url_info = "";

            for (const auto& pair : hits_and_misses) {
                std::string append = pair.first + ": HITS: " 
                    + std::to_string(pair.second.first) + ", MISSES: " + std::to_string(pair.second.second) + "\n"; 
                per_url_info += append;
            }

            res.set_content(
                "HITS: " + std::to_string(cache.GetHits()) + ", MISSES: " + std::to_string(cache.GetMisses()) + "\n" 
                + "Broken down by url:\n" + per_url_info, 
                "text/plain"
            );
        }},
        {"/favicon.ico", [&]() { /* Do nothing for this since we don't need a favicon. */ }},
    };

    if (!commands.contains(key)) {
        return false;
    }

    commands[key]();

    return true;
}

void ProxySpace::Proxy::HandleRequest(const httplib::Request &req, httplib::Response &res) {
    std::string key = req.target; 

    if (key.empty()) { 
        key = "/"; 
    }

    if (MatchesEndpoint(key, res)) {
        // Do not cache results from endpoint requests.
        return;
    }

    if (cache.HasUrl(key)) {
        const auto &cached = cache.get(key);
        res.status = cached.status;
        res.headers = cached.headers; 
        res.body = cached.body;

        cache.LogEvent(key, true);

        return;
    }

    auto origin_res = cli->Get(req.target.c_str());

    if (!origin_res) {
        std::string error_msg = "Proxy error: " + httplib::to_string(origin_res.error());
        res.status = 502;
        res.set_content(error_msg, "text/plain");
        return;
    }

    // Set the response data
    res.status = origin_res->status;
    res.headers = origin_res->headers;
    res.headers.insert({"X-Cache", "MISS"});
    res.body = origin_res->body;

    // Cache the response
    CacheSpace::CachedResponse cached;
    cached.status = origin_res->status;
    cached.headers = origin_res->headers;
    cached.headers.insert({"X-Cache", "HIT"});
    cached.body = origin_res->body;
    cache.put(key, cached);

    cache.LogEvent(key, false);
}

void ProxySpace::Proxy::TTLFunction() {
    const int interval = config.ttl * 1000 * 0.25;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        const int current_seconds = cache.GetCurrentSeconds();
        std::cout << "Doing a TTL check.\n";
        
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

void ProxySpace::Proxy::StartServer() {
    std::thread ttl_thread(&ProxySpace::Proxy::TTLFunction, this);
    ttl_thread.detach();

    svr.Get("/.*", [&](const httplib::Request &req, httplib::Response &res) {
        HandleRequest(req, res);
    });

    bool started = svr.listen("localhost", config.port);

    if (!started) {
        std::cerr << "ERROR: Failed to bind to port " << config.port << ". It is likely being used by something else.\n";
    }
}