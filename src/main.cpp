#include "httplib.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 5) {
        throw std::runtime_error("4 arguments are required: --port PORT_NUMBER --origin ORIGIN_URL");

        return 1;
    }

    int port_number = std::stoi(argv[2]);
    std::string base_url = std::string(argv[4]);
    httplib::Client cli(base_url);
    httplib::Server svr;

    // Listen for GET requests on the root path.
    svr.Get(".*", [&](const httplib::Request &req, httplib::Response &res) {
        std::string path = req.path;
        std::string full_url = base_url + path;
        res.set_content("Sending a request to " + full_url, "text/plain");
        auto request_res = cli.Get(path);

        if (!request_res) {
            res.set_content("Failed to load.", "text/plain");

            return;
        }

        res.set_content(request_res->body, "text/plain");
    });

    std::cout << "Server starting on http://localhost:" << port_number << "...\n";
    svr.listen("localhost", port_number); 
}