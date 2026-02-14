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
        if (!value.contains("port") || !value.contains("origin-url") || !value.contains("cache-size") || !value.contains("ttl")) {
            throw std::runtime_error("Config for " + key + " is missing required fields!");
        }

        ProxySpace::ProxyConfig config;

        // Make sure that the origin_url does not have "https://" in front. 
        std::string origin_url = value["origin-url"];

        if (origin_url.rfind("https://", 0) == 0) {
            origin_url = origin_url.substr(8);
        } else if (origin_url.rfind("http://", 0) == 0) {
            origin_url = origin_url.substr(7);
        }

        config.port = value["port"];
        config.origin_url = origin_url;
        config.cache_size = value["cache-size"];
        config.ttl = value["ttl"];

        // Add the routes
        if (value.contains("routes")) {
            for (const auto& route : value["routes"]) {
                if (!route.contains("prefix") || !route.contains("origin")) {
                    std::cout << "Route config is missing required fields!\n";
                }

                ProxySpace::RouteConfig route_config;
                route_config.prefix = route["prefix"];
                route_config.origin = route["origin"];
                config.routes.push_back(route_config);
            }
        }

        std::cout << "Creating proxy with port: " << config.port << ", origin url: " << config.origin_url 
            << ", cache size: " << config.cache_size << ", ttl: " << config.ttl << "\n";
        std::cout << "Routes:\n";

        for (const auto& route : config.routes) {
            std::cout << "Route prefix: " << route.prefix << ", origin: " << route.origin << "\n";
        }

        threads.emplace_back([config]() {
            ProxySpace::Proxy proxy{config};
            proxy.StartServer();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}