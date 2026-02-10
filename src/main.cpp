#include "httplib.h"
#include "Proxy.hpp"

// This is only needed for the initial creation, so I defined it here.
ProxySpace::ProxyConfig ParseArgs(int argc, char *argv[]) {
    ProxySpace::ProxyConfig config;
    int i{1};

    using CommandFunc = std::function<void()>;
    std::unordered_map<std::string, CommandFunc> commands = {
        {"--port", [&]() {
            config.port = std::stoi(argv[++i]);
        }},
        {"--origin-url", [&]() {
            std::string base_url = argv[++i];
            std::string ignore = "https://";

            if (base_url.starts_with(ignore)) {
                base_url = base_url.substr(ignore.size(), base_url.size() - ignore.size());
            }

            config.origin_url = base_url;
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
            throw std::runtime_error("Invalid command: " + command + "!");
        }

        commands[command]();
        i++;
    }

    assert(("Port number must be > 0", config.port > 0));
    assert(("Cache size must be > 0", config.cache_size > 0));
    assert(("TTL must be > 0", config.ttl > 0));
    assert(("Origin url must be non-empty", !config.origin_url.empty()));

    return config;
}

int main(int argc, char *argv[])
{
    if (argc < 5) {
        std::cout << "Usage: proxy --port <port> --origin-url <url>\n";
        return 1;   
    }

    ProxySpace::ProxyConfig config = ParseArgs(argc, argv);
    ProxySpace::Proxy proxy{config};
    proxy.StartServer();
}