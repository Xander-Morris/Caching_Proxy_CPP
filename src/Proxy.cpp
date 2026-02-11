#include "Proxy.hpp"

bool ProxySpace::Proxy::MatchesEndpoint(const std::string &key, httplib::Response &res) {
    using CommandFunc = std::function<void()>;
    std::unordered_map<std::string, CommandFunc> commands = {
        {"/stats", [&]() {
            auto hits_and_misses = cache.GetURLHitsAndMisses();

            if (hits_and_misses.size() == 0 && cache.GetCompliantMisses() == 0) {
                res.set_content(
                    "No cache activity yet.\n",
                    "text/plain"
                );
                return;
            }

            std::string per_url_info = "";

            for (const auto& pair : hits_and_misses) {
                std::string append = pair.first + ": Hits: " 
                    + std::to_string(pair.second.first) + ", Misses: " + std::to_string(pair.second.second) + "\n"; 
                per_url_info += append;
            }

            res.set_content(
                "Hits: " + std::to_string(cache.GetHits()) + "\nMisses: " + std::to_string(cache.GetMisses()) + "\n" 
                + "Compliant Misses: " + std::to_string(cache.GetCompliantMisses()) + "\n"
                + "Hits and misses (non-compliant) broken down by url:\n" + per_url_info, 
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

void ProxySpace::Proxy::LogMessage(const std::string &message) {
    std::cout << "[PORT " << config.port << "] " << message << "\n";
}

void ProxySpace::Proxy::HandleRequest(const httplib::Request &req, httplib::Response &res) {
    std::string key = req.target; 

    if (key.empty()) { 
        key = "/"; 
    }

    LogMessage("Received request for " + key);

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

    httplib::Headers headers;
    headers.insert({"Host", config.origin_url});
    headers.insert({"Connection", "close"});

    std::string body;
    bool too_large = false;

    auto origin_res = cli->Get(
        req.target.c_str(),
        headers,
        [&](const char* data, size_t data_length) {
            constexpr size_t MAX_RESPONSE_SIZE = 2 * 1024 * 1024;

            if (body.size() + data_length > MAX_RESPONSE_SIZE) {
                too_large = true;
                return false; // abort download
            }

            body.append(data, data_length);
            return true;
        }
    );

    if (!origin_res) {
        std::string error_msg = "Proxy error: " + httplib::to_string(origin_res.error());
        res.status = 502;
        res.set_content(error_msg, "text/plain");
        return;
    }

    if (too_large) {
        res.status = 413;
        res.set_content("Origin response too large", "text/plain");
        return;
    }

    std::optional<int> max_age;
    auto it = origin_res->headers.find("Cache-Control");

    if (it != origin_res->headers.end()) {
        max_age = ParseMaxAge(it->second);
    }

    res.status = origin_res->status;
    res.body = origin_res->body;

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

    res.headers.clear();

    httplib::Headers filtered_headers;
    for (const auto& [key, value] : origin_res->headers) {
        std::string lower = key;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (!hop_by_hop.contains(lower)) {
            filtered_headers.insert({key, value});
        }
    }

    filtered_headers.insert({"Content-Length", std::to_string(res.body.size())});

    res.headers = filtered_headers;
    res.headers.insert({"X-Cache", "MISS"});
    int now = cache.GetCurrentSeconds();
    int to_add = config.ttl;

    if (max_age) {
        to_add = int(*max_age);
    }

    // Do not cache it if the TTL is 0.
    if (to_add == 0) {
        cache.IncrementCompliantMisses();
        return;
    }

    CacheSpace::CachedResponse cached;
    cached.status = origin_res->status;
    cached.body = origin_res->body;
    cached.headers = filtered_headers;  
    cached.headers.insert({"X-Cache", "HIT"});
    cached.expires_at = now + to_add;
    cache.put(key, cached);
    cache.LogEvent(key, false); // Only log the miss if the request was meant to be cached in the first place.
}

void ProxySpace::Proxy::TTLFunction() {
    const int interval = 1000;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        const int current_seconds = cache.GetCurrentSeconds();
        
        while (cache.CheckHeapTop()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }
}

void ProxySpace::Proxy::StartServer() {
    std::thread ttl_thread(&ProxySpace::Proxy::TTLFunction, this);
    ttl_thread.detach();

    svr.Get("/.*", [&](const httplib::Request &req, httplib::Response &res) {
        HandleRequest(req, res);
    });

    svr.set_payload_max_length(1 * 1024 * 1024); // 1 MB limit for request bodies
    bool started = svr.listen("localhost", config.port, /*use_multithread=*/true);

    if (!started) {
        std::cerr << "ERROR: Failed to bind to port " << config.port << ". It is likely being used by something else.\n";
    }
}

// It is possible for the max age to be 0.
std::optional<int> ProxySpace::Proxy::ParseMaxAge(const std::string& cache_control) {
    LogMessage("Received cache control: " + cache_control);
    std::stringstream ss(cache_control);
    std::string directive;

    while (std::getline(ss, directive, ',')) {
        // Trim whitespace
        directive.erase(0, directive.find_first_not_of(" \t"));
        directive.erase(directive.find_last_not_of(" \t") + 1);

        // Case-insensitive compare
        std::string lower = directive;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower.rfind("max-age=", 0) == 0) {
            try {
                return std::stoi(lower.substr(8));
            } catch (...) {
                return std::nullopt;
            }
        } else if (lower.rfind("no-store", 0) == 0 || lower.rfind("no-cache", 0) == 0) {
            return 0;
        }
    }

    return std::nullopt;
}