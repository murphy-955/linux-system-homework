#include "include/network/tcp_server.h"
#include "include/communication_config.h"
#include <string>
#include <iostream>
#include <format>
#include <vector>
#include "include/protocols/PIAP.h"
#include "include/protocols/TITP.h"
#include <print>

int main() {
    tcp_server server(PORT);
    std::cout << std::format("Server Starts at: 0.0.0.0: {}\n", PORT);

    std::vector<int> client_fds;
    int fd = server.accept_connection();

    while(true) {
        
    }

    server.shutdown_server();
    std::cout << std::format("Server closes successfully!\n");

    return 0;
}