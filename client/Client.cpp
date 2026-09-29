#include "Client.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

namespace fts {

    Client::Client() : m_host("127.0.0.1"), m_port(DEFAULT_PORT), m_connected(false) {
    }

    Client::~Client() {
        disconnect();
    }

    bool Client::connectToServer(const std::string& host, int port) {
        m_host = host;
        m_port = port;

        std::cout << "[Client] Connecting to server at " << m_host << ":" << m_port << "...\n";
        if (!m_socket.connect(m_host, m_port)) {
            std::cerr << "[Client] ERROR: Connection failed.\n";
            m_connected = false;
            return false;
        }

        m_connected = true;
        std::cout << "[Client] Connected successfully to " << m_host << ":" << m_port << "!\n";
        return true;
    }

    bool Client::listFiles() {
        if (!m_connected) {
            std::cerr << "[Client] Not connected to server.\n";
            return false;
        }

        // Prepare LIST_FILES request header
        PacketHeader req{};
        req.magic = PROTOCOL_MAGIC;
        req.command = static_cast<uint8_t>(Command::LIST_FILES);
        req.status = static_cast<uint8_t>(StatusCode::OK);
        req.filenameLength = 0;
        req.payloadSize = 0;

        // Step 1: Send request header
        PacketHeader wireReq = serializeHeader(req);
        if (!m_socket.sendAll(&wireReq, sizeof(wireReq))) {
            std::cerr << "[Client] Failed to send LIST_FILES request.\n";
            return false;
        }

        // Step 2: Read response header
        PacketHeader resp{};
        if (!m_socket.receiveAll(&resp, sizeof(resp))) {
            std::cerr << "[Client] Failed to receive server response header.\n";
            return false;
        }

        resp = deserializeHeader(resp);

        if (resp.magic != PROTOCOL_MAGIC) {
            std::cerr << "[Client] Invalid magic from server.\n";
            return false;
        }

        // Step 3: Read response payload (file list text)
        if (resp.payloadSize > 0) {
            std::string fileList(static_cast<size_t>(resp.payloadSize), '\0');
            if (!m_socket.receiveAll(&fileList[0], fileList.size())) {
                std::cerr << "[Client] Failed to receive file list payload.\n";
                return false;
            }

            std::cout << "\n--- Files on Server ---\n";
            std::cout << fileList;
            std::cout << "-----------------------\n";
        } else {
            std::cout << "[Client] Server reports no files.\n";
        }

        return true;
    }

    bool Client::uploadFile(const std::string& localFilePath) {
        if (!m_connected) {
            std::cerr << "[Client] Not connected to server.\n";
            return false;
        }

        fs::path p(localFilePath);
        std::error_code ec;
        if (!fs::exists(p, ec) || !fs::is_regular_file(p, ec)) {
            std::cerr << "[Client] ERROR: Local file does not exist or is not a regular file: " << localFilePath << "\n";
            return false;
        }

        uint64_t fileSize = fs::file_size(p, ec);
        std::string filename = p.filename().string();

        std::ifstream inFile(p, std::ios::binary);
        if (!inFile.is_open()) {
            std::cerr << "[Client] ERROR: Could not open local file for reading: " << localFilePath << "\n";
            return false;
        }

        // Step 1: Send UPLOAD request header
        PacketHeader req{};
        req.magic = PROTOCOL_MAGIC;
        req.command = static_cast<uint8_t>(Command::UPLOAD_FILE);
        req.status = static_cast<uint8_t>(StatusCode::OK);
        req.filenameLength = static_cast<uint16_t>(filename.size());
        req.payloadSize = fileSize;

        PacketHeader wireReq = serializeHeader(req);
        if (!m_socket.sendAll(&wireReq, sizeof(wireReq))) {
            std::cerr << "[Client] Failed to send upload header.\n";
            return false;
        }

        // Step 2: Send filename
        if (!m_socket.sendAll(filename.data(), filename.size())) {
            std::cerr << "[Client] Failed to send filename.\n";
            return false;
        }

        std::cout << "[Client] Uploading '" << filename << "' (" << fileSize << " bytes)...\n";

        // Step 3: Stream file chunks (64 KB at a time)
        std::vector<char> buffer(BUFFER_CHUNK_SIZE);
        uint64_t bytesTransferred = 0;

        while (bytesTransferred < fileSize) {
            size_t toRead = static_cast<size_t>(std::min<uint64_t>(BUFFER_CHUNK_SIZE, fileSize - bytesTransferred));
            inFile.read(buffer.data(), toRead);
            std::streamsize bytesRead = inFile.gcount();
            if (bytesRead <= 0) {
                std::cerr << "\n[Client] ERROR: Failed reading local file stream.\n";
                return false;
            }

            if (!m_socket.sendAll(buffer.data(), static_cast<size_t>(bytesRead))) {
                std::cerr << "\n[Client] ERROR: Failed sending data chunk over socket.\n";
                return false;
            }

            bytesTransferred += static_cast<uint64_t>(bytesRead);
            printProgressBar(bytesTransferred, fileSize);
        }
        std::cout << "\n";

        // Step 4: Receive Server ACK
        PacketHeader resp{};
        if (!m_socket.receiveAll(&resp, sizeof(resp))) {
            std::cerr << "[Client] Failed to receive server upload ACK.\n";
            return false;
        }

        resp = deserializeHeader(resp);
        if (resp.status == static_cast<uint8_t>(StatusCode::OK)) {
            std::cout << "[Client] SUCCESS: File '" << filename << "' uploaded successfully!\n";
            return true;
        } else {
            std::cerr << "[Client] ERROR: Server rejected upload with status code: " 
                      << static_cast<int>(resp.status) << "\n";
            return false;
        }
    }

    bool Client::downloadFile(const std::string& remoteFilename, const std::string& localSavePath) {
        if (!m_connected) {
            std::cerr << "[Client] Not connected to server.\n";
            return false;
        }

        // Step 1: Send DOWNLOAD request header
        PacketHeader req{};
        req.magic = PROTOCOL_MAGIC;
        req.command = static_cast<uint8_t>(Command::DOWNLOAD_FILE);
        req.status = static_cast<uint8_t>(StatusCode::OK);
        req.filenameLength = static_cast<uint16_t>(remoteFilename.size());
        req.payloadSize = 0;

        PacketHeader wireReq = serializeHeader(req);
        if (!m_socket.sendAll(&wireReq, sizeof(wireReq))) {
            std::cerr << "[Client] Failed to send download header.\n";
            return false;
        }

        // Step 2: Send remote filename
        if (!m_socket.sendAll(remoteFilename.data(), remoteFilename.size())) {
            std::cerr << "[Client] Failed to send remote filename.\n";
            return false;
        }

        // Step 3: Receive server response header
        PacketHeader resp{};
        if (!m_socket.receiveAll(&resp, sizeof(resp))) {
            std::cerr << "[Client] Failed to receive server download response.\n";
            return false;
        }

        resp = deserializeHeader(resp);

        if (resp.magic != PROTOCOL_MAGIC) {
            std::cerr << "[Client] Invalid magic from server.\n";
            return false;
        }

        if (resp.status == static_cast<uint8_t>(StatusCode::FILE_NOT_FOUND)) {
            std::cerr << "[Client] ERROR: Remote file '" << remoteFilename << "' not found on server!\n";
            return false;
        }

        if (resp.status != static_cast<uint8_t>(StatusCode::OK)) {
            std::cerr << "[Client] ERROR: Server error on download. Status code: " 
                      << static_cast<int>(resp.status) << "\n";
            return false;
        }

        uint64_t fileSize = resp.payloadSize;
        std::string destination = localSavePath.empty() ? remoteFilename : localSavePath;

        std::ofstream outFile(destination, std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "[Client] ERROR: Failed to create destination file: " << destination << "\n";
            return false;
        }

        std::cout << "[Client] Downloading '" << remoteFilename << "' (" << fileSize << " bytes)...\n";

        // Step 4: Stream incoming chunks into file
        std::vector<char> buffer(BUFFER_CHUNK_SIZE);
        uint64_t bytesTransferred = 0;

        while (bytesTransferred < fileSize) {
            size_t toReceive = static_cast<size_t>(std::min<uint64_t>(BUFFER_CHUNK_SIZE, fileSize - bytesTransferred));
            if (!m_socket.receiveAll(buffer.data(), toReceive)) {
                std::cerr << "\n[Client] ERROR: Network dropped while receiving chunk.\n";
                return false;
            }

            outFile.write(buffer.data(), toReceive);
            bytesTransferred += toReceive;
            printProgressBar(bytesTransferred, fileSize);
        }
        std::cout << "\n";

        std::cout << "[Client] SUCCESS: Download complete! Saved to: " << destination << "\n";
        return true;
    }

    void Client::printProgressBar(uint64_t completed, uint64_t total) {
        if (total == 0) return;
        const int barWidth = 30;
        float progress = static_cast<float>(completed) / static_cast<float>(total);
        int pos = static_cast<int>(barWidth * progress);

        std::cout << "\r[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        int percent = static_cast<int>(progress * 100.0f);
        std::cout << "] " << std::setw(3) << percent << "% (" 
                  << completed << "/" << total << " bytes)" << std::flush;
    }

    void Client::disconnect() {
        if (m_connected) {
            PacketHeader req{};
            req.magic = PROTOCOL_MAGIC;
            req.command = static_cast<uint8_t>(Command::DISCONNECT);
            PacketHeader wireReq = serializeHeader(req);
            m_socket.sendAll(&wireReq, sizeof(wireReq));

            m_socket.close();
            m_connected = false;
            std::cout << "[Client] Disconnected from server.\n";
        }
    }

    bool Client::isConnected() const {
        return m_connected;
    }

} // namespace fts
