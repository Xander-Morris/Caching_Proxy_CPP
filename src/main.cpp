#include "httplib.h"
#include "Cache.hpp"

void HandleRequest(const httplib::Request &req, httplib::Response &res, Cache &cache, httplib::Client &cli) {
    std::string key = req.target; 

    if (key.empty()) { 
        key = "/"; 
    }

    if (cache.HasUrl(key)) {
        const auto &cached = cache.get(key);
        res.status = cached.status;
        res.headers = cached.headers; 
        res.body = cached.body;

        std::cout << "There was a cache hit!\n";

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

    std::cout << "There was a cache miss, so we inserted into the cache!\n";
}

void StartServer(Cache &cache, const std::string &host, int port_number) {
    httplib::Client cli(host.c_str());
    httplib::Server svr;

    svr.Get("/.*", [&](const httplib::Request &req, httplib::Response &res) {
        std::thread([req, &res, &cache, &cli]() {
            HandleRequest(req, res, cache, cli);
        }).detach();
    });

    bool started = svr.listen("localhost", port_number);

    if (!started) {
        std::cerr << "ERROR: Failed to bind to port " << port_number << ". It is likely being used by something else.\n";
    }
}

int main(int argc, char *argv[])
{
    if (argc < 5) {
        std::cout << "Usage: proxy --port <port> --origin-url <url>\n";
        return 1;
    }

    int port = 0;
    std::string origin_url;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        }
        else if (arg == "--origin-url" && i + 1 < argc) {
            origin_url = argv[++i];
        }
    }

    if (port == 0 || origin_url.empty()) {
        std::cerr << "Missing required arguments\n";
        return 1;
    }

    if (origin_url.back() == '/') {
        origin_url.pop_back();
    }

    if (origin_url.starts_with("http://")) {
        origin_url = origin_url.substr(7);
    }
    else if (origin_url.starts_with("https://")) {
        origin_url = origin_url.substr(8);
    }

    Cache cache(15);
    StartServer(cache, origin_url, port);
}