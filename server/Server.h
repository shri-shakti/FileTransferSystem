#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <vector>
#include <thread>
#include "Socket.h"
#include "Protocol.h"

namespace fts {

    /**
     * @brief Multi-Threaded Server class.
     * 
     * Concurrency Model:
     * - Spawns a dedicated std::thread for every connected client.
     * - Protects shared storage access with std::mutex and std::lock_guard.
     * - Synchronizes console output across worker threads with std::mutex.
     * - Tracks active client count with std::atomic<int>.
     */
    class Server {
    public:
        explicit Server(int port = DEFAULT_PORT, const std::string& storageDir = "storage");
        ~Server();

        // Starts the multi-threaded server listening loop
        bool start();

        // Stops the server gracefully
        void stop();

        // Get count of currently connected clients
        int getActiveClientCount() const { return m_activeClients.load(); }

    private:
        // Worker function executed on a dedicated thread for each client
        void handleClient(Socket clientSocket, std::string clientIp, uint16_t clientPort);

        // Command Handlers (Thread-safe)
        void handleListFiles(Socket& clientSocket, const std::string& clientIp);
        void handleUploadFile(Socket& clientSocket, const PacketHeader& header, const std::string& clientIp);
        void handleDownloadFile(Socket& clientSocket, const PacketHeader& header, const std::string& clientIp);

        // Synchronized logging to prevent mangled console output
        void log(const std::string& message);

        int m_port;
        std::string m_storageDir;
        Socket m_listeningSocket;
        std::atomic<bool> m_running;
        std::atomic<int> m_activeClients;

        // Synchronization Primitives
        std::mutex m_storageMutex; // Protects file operations in storage/
        std::mutex m_logMutex;     // Protects std::cout from interleaved prints
    };

} // namespace fts
