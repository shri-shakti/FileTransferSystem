#pragma once

#include <string>
#include "Socket.h"
#include "Protocol.h"

namespace fts {

    /**
     * @brief Client class responsible for connecting to the Server and
     * executing commands (Listing files, Uploading, Downloading).
     */
    class Client {
    public:
        Client();
        ~Client();

        // Connect to the File Transfer Server
        bool connectToServer(const std::string& host = "127.0.0.1", int port = DEFAULT_PORT);

        // Request list of files stored on the server
        bool listFiles();

        // Upload a local file to the server in 64 KB chunks
        bool uploadFile(const std::string& localFilePath);

        // Download a file from the server in 64 KB chunks
        bool downloadFile(const std::string& remoteFilename, const std::string& localSavePath = "");

        // Disconnect gracefully from server
        void disconnect();

        bool isConnected() const;

    private:
        // Helper to display a clean progress bar in console
        static void printProgressBar(uint64_t completed, uint64_t total);

        Socket m_socket;
        std::string m_host;
        int m_port;
        bool m_connected;
    };

} // namespace fts
