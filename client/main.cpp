#include <iostream>
#include <string>
#include <sstream>
#include "Client.h"

void printUsage() {
    std::cout << "\nAvailable Commands:\n";
    std::cout << "  list                 - View all files stored on the server\n";
    std::cout << "  upload <filepath>    - Upload a local file to the server\n";
    std::cout << "  download <filename>  - Download a file from the server\n";
    std::cout << "  help                 - Show this help menu\n";
    std::cout << "  exit                 - Disconnect and exit\n\n";
}

int main(int argc, char* argv[]) {
    std::cout << "===================================================\n";
    std::cout << "       File Transfer System - Interactive Client    \n";
    std::cout << "===================================================\n";

    std::string host = "127.0.0.1";
    int port = fts::DEFAULT_PORT;

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = std::stoi(argv[2]);

    fts::Client client;
    if (!client.connectToServer(host, port)) {
        std::cerr << "Failed to connect to " << host << ":" << port << ". Is the server running?\n";
        return 1;
    }

    printUsage();

    std::string line;
    while (true) {
        std::cout << "fts> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "list") {
            client.listFiles();
        }
        else if (cmd == "upload") {
            std::string filePath;
            iss >> filePath;
            if (filePath.empty()) {
                std::cout << "Usage: upload <path_to_file>\n";
            } else {
                client.uploadFile(filePath);
            }
        }
        else if (cmd == "download") {
            std::string remoteFilename, localSavePath;
            iss >> remoteFilename >> localSavePath;
            if (remoteFilename.empty()) {
                std::cout << "Usage: download <remote_filename> [optional_destination_path]\n";
            } else {
                client.downloadFile(remoteFilename, localSavePath);
            }
        }
        else if (cmd == "help") {
            printUsage();
        }
        else if (cmd == "exit" || cmd == "quit") {
            break;
        }
        else {
            std::cout << "Unknown command: '" << cmd << "'. Type 'help' for available commands.\n";
        }
    }

    client.disconnect();
    std::cout << "Goodbye!\n";
    return 0;
}
