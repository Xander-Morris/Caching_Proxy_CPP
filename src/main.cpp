#include "httplib.h"
#include "Cache.hpp"

void StartServer(Cache &cache, const std::string &host, int port_number) {
    httplib::Client cli(host.c_str());
    httplib::Server svr;

    svr.Get("/.*", [&](const httplib::Request &req, httplib::Response &res) {
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
    });

    svr.listen("localhost", port_number);
}

int main(int argc, char *argv[])
{
    Cache cache(15);
    int i = 1;

    while (i < argc)
    {
        std::string keyword = argv[i];

        if (keyword == "--port")
        {
            int port_number = std::stoi(argv[++i]);
            i += 2;
            std::string base_url = std::string(argv[i]);

            if (!base_url.empty() && base_url.back() != '/')
            {
                base_url += '/';
                std::cout << "Normalized base_url to: " << base_url << "\n";
            }

            std::string host = base_url;
            if (host.starts_with("http://"))
            {
                host = host.substr(7);
            }
            if (host.starts_with("https://"))
            {
                host = host.substr(8);
            }
            if (host.ends_with('/'))
            {
                host.pop_back();
            }

            StartServer(cache, host, port_number);
        }
        else if (keyword == "--clear-cache")
        {
            cache.clear();
            i++;  
        }
        else
        {
            i++;  
        }
    }
}