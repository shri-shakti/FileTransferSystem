#include <iostream>
#include "Server.h"

int main() {
    std::cout << "==========================================\n";
    std::cout << "   File Transfer System - SERVER DAEMON   \n";
    std::cout << "==========================================\n";

    fts::Server server(fts::DEFAULT_PORT, "storage");
    if (!server.start()) {
        std::cerr << "[Server] Failed to run server.\n";
        return 1;
    }

    return 0;
}
