#include "httplib.h"
#include "Proxy.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

int main()
{
    std::ifstream config_file("cache_config.json");

    if (!config_file.is_open()) {
        throw std::runtime_error("Could not open config file!");
    }

    using json = nlohmann::json;
    json results = json::parse(config_file);
    std::vector<std::thread> threads;

    for (const auto& [key, value] : results.items()) {
        ProxySpace::ProxyConfig config;
        config.port = value["port"];
        config.origin_url = value["origin-url"];
        config.cache_size = value["cache-size"];
        config.ttl = value["ttl"];

        std::cout << "Creating proxy with port: " << config.port << ", origin url: " << config.origin_url 
            << ", cache size: " << config.cache_size << ", ttl: " << config.ttl << "\n";

        threads.emplace_back([config]() {
            ProxySpace::Proxy proxy{config};
            proxy.StartServer();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}