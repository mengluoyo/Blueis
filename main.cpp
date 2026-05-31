#include <iostream>
#include "src/server/server.h"

int main() {
    blueis::TcpServer server(6380);

    if (!server.start()) {
        std::cerr << "Failed to start Blueis server" << std::endl;
        return 1;
    }

    std::cout << "Blueis v1.0.0 ready. Port: 6380" << std::endl;
    std::cout << "Connect via: telnet 127.0.0.1 6380" << std::endl;

    server.run();
    return 0;
}
