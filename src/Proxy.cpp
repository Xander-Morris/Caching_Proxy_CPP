#include "Proxy.hpp"
#include <nlohmann/json.hpp>

void ProxySpace::Proxy::BuildClients() {
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

void ProxySpace::Proxy::BuildEndpoints() {
    endpoints["/stats"] = [this](const httplib::Request& req, httplib::Response& res) {
        auto accept_it = req.headers.find("Accept");

        if (accept_it != req.headers.end() && accept_it->second.find("application/json") != std::string::npos) {
            nlohmann::json j;
            j["hits"] = cache.GetHits();
            j["misses"] = cache.GetMisses();
            j["compliant_misses"] = cache.GetCompliantMisses();
            auto hits_and_misses = cache.GetURLHitsAndMisses();
            nlohmann::json url_info;

            for (const auto& pair : hits_and_misses) {
                url_info[pair.first] = {
                    {"hits", pair.second.first},
                    {"misses", pair.second.second}
                };
            }

            j["url_hits_and_misses"] = url_info;
            res.set_content(j.dump(4), "application/json");

            return;
        }

        auto hits_and_misses = cache.GetURLHitsAndMisses();

        if (hits_and_misses.empty() && cache.GetCompliantMisses() == 0) {
            res.set_content("No cache activity yet.\n", "text/plain");
            return;
        }

        std::string per_url_info;

        for (const auto& pair : hits_and_misses) {
            per_url_info += pair.first + ": Hits: "
                + std::to_string(pair.second.first)
                + ", Misses: " + std::to_string(pair.second.second) + "\n";
        }

        res.set_content(
            "Hits: " + std::to_string(cache.GetHits()) + "\n"
            "Misses: " + std::to_string(cache.GetMisses()) + "\n"
            "Compliant Misses: " + std::to_string(cache.GetCompliantMisses()) + "\n"
            "Hits and misses (non-compliant) broken down by url:\n" + per_url_info,
            "text/plain"
        );
    };

    endpoints["/clear-cache"] = [this](const httplib::Request&, httplib::Response& res) {
        cache.clear();
        res.set_content("Cache cleared.\n", "text/plain");
    };

    endpoints["/healthz"] = [this](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
    };

    // no-op
    endpoints["/favicon.ico"] = [this](const httplib::Request&, httplib::Response&) {};
}

bool ProxySpace::Proxy::MatchesEndpoint(const std::string &key, const httplib::Request &req, httplib::Response &res) {
    auto it = endpoints.find(key);
    if (it == endpoints.end()) return false;

    it->second(req, res);

    return true;
}

void ProxySpace::Proxy::LogMessage(const std::string &message) {
    std::cout << "[PORT " << config.port << "] " << message << "\n";
}

// Returns true if we handled the request completely.
bool ProxySpace::Proxy::CheckCacheForResponse(const std::string &key, httplib::Response &res) {
    if (!cache.HasUrl(key)) return false;

    auto cached = cache.get(key);
    int now = cache.GetCurrentSeconds();

    if (cached.expires_at < now) {
        httplib::Headers headers;
        headers.insert({"Host", config.origin_url});
        headers.insert({"Connection", "close"});

        if (cached.headers.contains("ETag")) {
            auto it = cached.headers.find("ETag");

            if (it != cached.headers.end()) {
                headers.insert({"If-None-Match", it->second});
            }
        }

        if (cached.headers.contains("Last-Modified")) {
            auto it = cached.headers.find("Last-Modified");
            
            if (it != cached.headers.end()) {
                headers.insert({"If-Modified-Since", it->second});
            }
        }

        auto origin_res = clients.at(SelectOrigin(key))->Get(key.c_str(), headers);

        if (!origin_res) {
            res.status = 502;
            res.set_content("Proxy error: conditional request failed", "text/plain");
            return true;
        }

        if (origin_res->status == 304) {
            cached.expires_at = now + config.ttl;
            cache.put(key, cached);

            res.status = cached.status;
            res.headers = cached.headers;
            res.body = cached.body;

            return true;
        }
    } else {
        res.status = cached.status;
        res.headers = cached.headers;
        res.body = cached.body;
        
        return true;
    }

    return false;
}

std::string ProxySpace::Proxy::MakeCacheKey(const httplib::Request& req) const {
    std::string key = req.target;

    if (key.empty()) { 
        key = "/";
        return key; 
    }

    auto vary_it = req.headers.find("Vary");

    if (vary_it != req.headers.end()) {
        std::string vary_headers = vary_it->second;
        std::stringstream ss(vary_headers);
        std::string header_name;

        while (std::getline(ss, header_name, ',')) {
            header_name.erase(0, header_name.find_first_not_of(" \t"));
            header_name.erase(header_name.find_last_not_of(" \t") + 1);
            auto hdr_it = req.headers.find(header_name);

            if (hdr_it != req.headers.end()) {
                key += "|" + header_name + "=" + hdr_it->second;
            }
        }
    }

    return key;
}

void ProxySpace::Proxy::HandleRequest(const httplib::Request &req, httplib::Response &res) {
    std::string key = MakeCacheKey(req);
    LogMessage("Received request for " + key);

    if (MatchesEndpoint(key, req, res)) {
        // Do not cache results from endpoint requests.
        return;
    }

    if (CheckCacheForResponse(key, res)) {
        cache.LogEvent(key, true);
        return;
    }

    httplib::Headers headers;
    headers.insert({"Host", config.origin_url});
    headers.insert({"Connection", "close"});
    std::string origin_host = SelectOrigin(key);
    std::cout << "Selected origin: " << origin_host << " for request path: " << key << "\n";

    if (!clients.contains(origin_host)) {
        res.status = 502;
        res.set_content("Proxy error: unknown origin", "text/plain");
        return;
    }

    auto cli = clients.at(origin_host).get();
    std::string body;
    bool too_large = false;

    auto origin_res = cli->Get(
        key.c_str(),
        headers,
        [&](const char* data, size_t length) {
            constexpr size_t MAX_RESPONSE_SIZE = 2 * 1024 * 1024;

            if (body.size() + length > MAX_RESPONSE_SIZE) {
                too_large = true;
                return false; // abort download
            }

            body.append(data, length);
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
    auto cache_it = origin_res->headers.find("Cache-Control");

    if (cache_it != origin_res->headers.end()) {
        max_age = ParseMaxAge(cache_it->second);
    }

    res.status = origin_res->status;
    res.body = origin_res->body;
    res.headers.clear();
    httplib::Headers filtered_headers;

    for (const auto& [key, value] : origin_res->headers) {
        std::string lower = key;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (!ProxySpace::hop_by_hop.contains(lower)) {
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

    while (is_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));        
        while (cache.CheckHeapTop()) {}
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

std::string ProxySpace::Proxy::SelectOrigin(const std::string& path) const {
    for (const auto& route : config.routes) {
        if (path.rfind(route.prefix, 0) == 0) {
            return route.origin;
        }
    }

    return config.origin_url; // default
}