#include "Server.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace fts {

    Server::Server(int port, const std::string& storageDir)
        : m_port(port), m_storageDir(storageDir), m_running(false), m_activeClients(0) {
        
        std::error_code ec;
        if (!fs::exists(m_storageDir, ec)) {
            fs::create_directories(m_storageDir, ec);
            log("[Server] Created storage directory: " + m_storageDir);
        }
    }

    Server::~Server() {
        stop();
    }

    void Server::log(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_logMutex);
        std::cout << message << std::endl;
    }

    bool Server::start() {
        log("[Server] Binding to port " + std::to_string(m_port) + "...");
        if (!m_listeningSocket.bindAndListen(m_port)) {
            log("[Server] ERROR: Failed to listen on port " + std::to_string(m_port));
            return false;
        }

        m_running = true;
        log("[Server] Multi-threaded server listening on port " + std::to_string(m_port) + ".");
        log("[Server] Ready to accept concurrent client connections...");

        while (m_running) {
            std::string clientIp;
            uint16_t clientPort = 0;

            Socket clientSocket = m_listeningSocket.accept(clientIp, clientPort);
            if (!clientSocket.isValid()) {
                if (m_running) {
                    log("[Server] Failed to accept client connection.");
                }
                break;
            }

            m_activeClients++;
            log("[Server] >>> New client connected from " + clientIp + ":" + std::to_string(clientPort) 
                + " (Active clients: " + std::to_string(m_activeClients.load()) + ")");

            // Modern C++: Spawn a dedicated thread for each client so multiple clients
            // can upload and download files at the exact same time!
            std::thread clientThread([this, sock = std::move(clientSocket), ip = clientIp, port = clientPort]() mutable {
                handleClient(std::move(sock), ip, port);
            });

            // Detach thread to allow independent execution in background
            clientThread.detach();
        }

        return true;
    }

    void Server::stop() {
        if (m_running) {
            m_running = false;
            m_listeningSocket.close();
            log("[Server] Server stopped.");
        }
    }

    void Server::handleClient(Socket clientSocket, std::string clientIp, uint16_t clientPort) {
        // RAII Guard: Guarantees active client count decrements even if an error or disconnect occurs
        struct ClientCounterGuard {
            std::atomic<int>& counter;
            std::string endpoint;
            Server& srv;
            ClientCounterGuard(std::atomic<int>& c, std::string ep, Server& s) : counter(c), endpoint(ep), srv(s) {}
            ~ClientCounterGuard() {
                counter--;
                srv.log("[Server] Client " + endpoint + " disconnected. (Active clients: " + std::to_string(counter.load()) + ")");
            }
        } guard(m_activeClients, clientIp + ":" + std::to_string(clientPort), *this);

        while (true) {
            PacketHeader header{};
            
            // Read 16-byte fixed packet header
            if (!clientSocket.receiveAll(&header, sizeof(header))) {
                break;
            }

            header = deserializeHeader(header);

            if (header.magic != PROTOCOL_MAGIC) {
                log("[Server] Corrupt protocol magic from " + clientIp + ". Disconnecting.");
                break;
            }

            Command cmd = static_cast<Command>(header.command);

            if (cmd == Command::LIST_FILES) {
                handleListFiles(clientSocket, clientIp);
            }
            else if (cmd == Command::UPLOAD_FILE) {
                handleUploadFile(clientSocket, header, clientIp);
            }
            else if (cmd == Command::DOWNLOAD_FILE) {
                handleDownloadFile(clientSocket, header, clientIp);
            }
            else if (cmd == Command::DISCONNECT) {
                log("[Server] Client " + clientIp + " requested disconnect.");
                break;
            }
            else {
                log("[Server] Unknown command from " + clientIp + ": " + std::to_string(static_cast<int>(cmd)));
                break;
            }
        }
    }

    void Server::handleListFiles(Socket& clientSocket, const std::string& clientIp) {
        log("[Server] Processing LIST_FILES from " + clientIp);

        std::string fileListStr;
        size_t fileCount = 0;

        // Thread-Safety: Lock mutex while reading directory so concurrent uploads don't cause race conditions
        {
            std::lock_guard<std::mutex> lock(m_storageMutex);
            std::ostringstream oss;
            std::error_code ec;

            for (const auto& entry : fs::directory_iterator(m_storageDir, ec)) {
                if (entry.is_regular_file()) {
                    oss << entry.path().filename().string() << " (" 
                        << entry.file_size() << " bytes)\n";
                    fileCount++;
                }
            }

            if (fileCount == 0) {
                oss << "(Storage folder is currently empty)\n";
            }
            fileListStr = oss.str();
        }

        PacketHeader responseHeader{};
        responseHeader.magic = PROTOCOL_MAGIC;
        responseHeader.command = static_cast<uint8_t>(Command::LIST_FILES);
        responseHeader.status = static_cast<uint8_t>(StatusCode::OK);
        responseHeader.filenameLength = 0;
        responseHeader.payloadSize = fileListStr.size();

        PacketHeader wireHeader = serializeHeader(responseHeader);
        clientSocket.sendAll(&wireHeader, sizeof(wireHeader));

        if (!fileListStr.empty()) {
            clientSocket.sendAll(fileListStr.data(), fileListStr.size());
        }

        log("[Server] Sent listing (" + std::to_string(fileCount) + " files) to " + clientIp);
    }

    void Server::handleUploadFile(Socket& clientSocket, const PacketHeader& header, const std::string& clientIp) {
        if (header.filenameLength == 0) {
            log("[Server] Upload missing filename from " + clientIp);
            return;
        }

        // Read filename string
        std::string rawFilename(header.filenameLength, '\0');
        if (!clientSocket.receiveAll(&rawFilename[0], rawFilename.size())) {
            log("[Server] Failed to receive filename during upload from " + clientIp);
            return;
        }

        // Sanitize to prevent directory traversal
        std::string safeFilename = fs::path(rawFilename).filename().string();
        fs::path destPath = fs::path(m_storageDir) / safeFilename;

        log("[Server] Starting concurrent upload from " + clientIp + ": '" 
            + safeFilename + "' (" + std::to_string(header.payloadSize) + " bytes)");

        // Thread-Safety: Lock storage mutex while creating/writing the file
        std::lock_guard<std::mutex> lock(m_storageMutex);

        std::ofstream outFile(destPath, std::ios::binary);
        if (!outFile.is_open()) {
            log("[Server] Failed to create destination file: " + destPath.string());

            PacketHeader errResp{};
            errResp.magic = PROTOCOL_MAGIC;
            errResp.command = static_cast<uint8_t>(Command::UPLOAD_FILE);
            errResp.status = static_cast<uint8_t>(StatusCode::SERVER_ERROR);
            PacketHeader wireErr = serializeHeader(errResp);
            clientSocket.sendAll(&wireErr, sizeof(wireErr));
            return;
        }

        std::vector<char> buffer(BUFFER_CHUNK_SIZE);
        uint64_t bytesRemaining = header.payloadSize;

        while (bytesRemaining > 0) {
            size_t toReceive = static_cast<size_t>(std::min<uint64_t>(BUFFER_CHUNK_SIZE, bytesRemaining));
            if (!clientSocket.receiveAll(buffer.data(), toReceive)) {
                log("[Server] Network dropped during upload of '" + safeFilename + "' from " + clientIp);
                outFile.close();
                fs::remove(destPath);
                return;
            }

            outFile.write(buffer.data(), toReceive);
            bytesRemaining -= toReceive;
        }

        outFile.close();
        log("[Server] Upload successfully completed for '" + safeFilename + "' from " + clientIp);

        // Send OK ACK
        PacketHeader ackHeader{};
        ackHeader.magic = PROTOCOL_MAGIC;
        ackHeader.command = static_cast<uint8_t>(Command::UPLOAD_FILE);
        ackHeader.status = static_cast<uint8_t>(StatusCode::OK);
        ackHeader.filenameLength = 0;
        ackHeader.payloadSize = 0;

        PacketHeader wireAck = serializeHeader(ackHeader);
        clientSocket.sendAll(&wireAck, sizeof(wireAck));
    }

    void Server::handleDownloadFile(Socket& clientSocket, const PacketHeader& header, const std::string& clientIp) {
        if (header.filenameLength == 0) {
            log("[Server] Download missing filename from " + clientIp);
            return;
        }

        std::string rawFilename(header.filenameLength, '\0');
        if (!clientSocket.receiveAll(&rawFilename[0], rawFilename.size())) {
            log("[Server] Failed to receive filename during download from " + clientIp);
            return;
        }

        std::string safeFilename = fs::path(rawFilename).filename().string();
        fs::path srcPath = fs::path(m_storageDir) / safeFilename;

        // Thread-Safety: Protect file access while reading
        std::lock_guard<std::mutex> lock(m_storageMutex);

        std::error_code ec;
        if (!fs::exists(srcPath, ec) || !fs::is_regular_file(srcPath, ec)) {
            log("[Server] Requested file not found: '" + safeFilename + "' for " + clientIp);

            PacketHeader notFoundResp{};
            notFoundResp.magic = PROTOCOL_MAGIC;
            notFoundResp.command = static_cast<uint8_t>(Command::DOWNLOAD_FILE);
            notFoundResp.status = static_cast<uint8_t>(StatusCode::FILE_NOT_FOUND);
            notFoundResp.filenameLength = 0;
            notFoundResp.payloadSize = 0;

            PacketHeader wireHdr = serializeHeader(notFoundResp);
            clientSocket.sendAll(&wireHdr, sizeof(wireHdr));
            return;
        }

        uint64_t fileSize = fs::file_size(srcPath, ec);
        std::ifstream inFile(srcPath, std::ios::binary);
        if (!inFile.is_open()) {
            log("[Server] Could not open file for download: " + srcPath.string());

            PacketHeader errResp{};
            errResp.magic = PROTOCOL_MAGIC;
            errResp.command = static_cast<uint8_t>(Command::DOWNLOAD_FILE);
            errResp.status = static_cast<uint8_t>(StatusCode::SERVER_ERROR);
            PacketHeader wireHdr = serializeHeader(errResp);
            clientSocket.sendAll(&wireHdr, sizeof(wireHdr));
            return;
        }

        log("[Server] Streaming '" + safeFilename + "' (" + std::to_string(fileSize) + " bytes) to " + clientIp);

        // Send OK response header
        PacketHeader respHeader{};
        respHeader.magic = PROTOCOL_MAGIC;
        respHeader.command = static_cast<uint8_t>(Command::DOWNLOAD_FILE);
        respHeader.status = static_cast<uint8_t>(StatusCode::OK);
        respHeader.filenameLength = 0;
        respHeader.payloadSize = fileSize;

        PacketHeader wireResp = serializeHeader(respHeader);
        clientSocket.sendAll(&wireResp, sizeof(wireResp));

        // Stream file chunks
        std::vector<char> buffer(BUFFER_CHUNK_SIZE);
        uint64_t bytesTransferred = 0;

        while (bytesTransferred < fileSize) {
            size_t toRead = static_cast<size_t>(std::min<uint64_t>(BUFFER_CHUNK_SIZE, fileSize - bytesTransferred));
            inFile.read(buffer.data(), toRead);
            std::streamsize bytesRead = inFile.gcount();
            if (bytesRead <= 0) break;

            if (!clientSocket.sendAll(buffer.data(), static_cast<size_t>(bytesRead))) {
                log("[Server] Connection dropped during download stream to " + clientIp);
                return;
            }

            bytesTransferred += static_cast<uint64_t>(bytesRead);
        }

        log("[Server] Finished streaming '" + safeFilename + "' to " + clientIp);
    }

} // namespace fts
