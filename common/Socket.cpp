#include "Socket.h"
#include <iostream>
#include <mutex>
#include <cstring>

namespace fts {

    // -----------------------------------------------------------------------------
    // Winsock Initialization (Singleton pattern)
    // -----------------------------------------------------------------------------
    static std::once_flag g_winsockInitFlag;

    void NetworkContext::initialize() {
#ifdef _WIN32
        std::call_once(g_winsockInitFlag, []() {
            WSADATA wsaData;
            int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                std::cerr << "[NetworkContext] WSAStartup failed with error: " << result << "\n";
            }
        });
#endif
    }

    void NetworkContext::cleanup() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // -----------------------------------------------------------------------------
    // RAII Socket Implementation
    // -----------------------------------------------------------------------------
    Socket::Socket() : m_handle(INVALID_SOCKET) {
        NetworkContext::initialize();
    }

    Socket::Socket(SocketHandle handle) : m_handle(handle) {
        NetworkContext::initialize();
    }

    Socket::~Socket() {
        close();
    }

    // Move Constructor: Take ownership from 'other' and invalidate 'other'
    Socket::Socket(Socket&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_SOCKET;
    }

    // Move Assignment Operator: Clean up our current handle, then adopt 'other'
    Socket& Socket::operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_SOCKET;
        }
        return *this;
    }

    bool Socket::create() {
        close(); // close existing if any
        m_handle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_handle == INVALID_SOCKET) {
            std::cerr << "[Socket] Failed to create socket.\n";
            return false;
        }
        return true;
    }

    bool Socket::bindAndListen(int port, int backlog) {
        if (!isValid() && !create()) {
            return false;
        }

        // Allow address reuse to prevent "Address already in use" errors during quick restarts
        int opt = 1;
#ifdef _WIN32
        ::setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
        ::setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY; // Listen on all network interfaces (0.0.0.0)
        serverAddr.sin_port = htons(static_cast<uint16_t>(port));

        if (::bind(m_handle, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "[Socket] Bind failed on port " << port << ".\n";
            close();
            return false;
        }

        if (::listen(m_handle, backlog) == SOCKET_ERROR) {
            std::cerr << "[Socket] Listen failed.\n";
            close();
            return false;
        }

        return true;
    }

    Socket Socket::accept(std::string& clientIp, uint16_t& clientPort) {
        if (!isValid()) {
            return Socket(INVALID_SOCKET);
        }

        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);

        SocketHandle clientHandle = ::accept(m_handle, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
        if (clientHandle == INVALID_SOCKET) {
            return Socket(INVALID_SOCKET);
        }

        char ipBuffer[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &(clientAddr.sin_addr), ipBuffer, INET_ADDRSTRLEN);
        clientIp = ipBuffer;
        clientPort = ntohs(clientAddr.sin_port);

        return Socket(clientHandle); // Return newly created Socket wrapping the accepted client
    }

    bool Socket::connect(const std::string& host, int port) {
        if (!isValid() && !create()) {
            return false;
        }

        // Resolve host (supports both IP addresses like "127.0.0.1" and hostnames like "localhost")
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        std::string portStr = std::to_string(port);
        if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || res == nullptr) {
            std::cerr << "[Socket] Failed to resolve host: " << host << "\n";
            return false;
        }

        int connResult = ::connect(m_handle, res->ai_addr, static_cast<int>(res->ai_addrlen));
        freeaddrinfo(res);

        if (connResult == SOCKET_ERROR) {
            std::cerr << "[Socket] Failed to connect to " << host << ":" << port << "\n";
            close();
            return false;
        }

        return true;
    }

    // Critical Networking Concept:
    // TCP sends data as an unbounded stream of bytes. Calling ::send() once does NOT guarantee
    // that all bytes are transmitted. sendAll() loops until every single byte is sent!
    bool Socket::sendAll(const void* data, size_t length) {
        if (!isValid()) return false;

        const char* buffer = static_cast<const char*>(data);
        size_t totalBytesSent = 0;

        while (totalBytesSent < length) {
            int sent = ::send(m_handle, buffer + totalBytesSent, static_cast<int>(length - totalBytesSent), 0);
            if (sent <= 0) {
                // Connection closed or error occurred
                return false;
            }
            totalBytesSent += sent;
        }

        return true;
    }

    // Critical Networking Concept:
    // Similar to send(), ::recv() returns whatever bytes are currently in the network buffer.
    // If you need 16 bytes for a header, recv() might return 4 bytes first.
    // receiveAll() keeps reading until the exact requested length is acquired.
    bool Socket::receiveAll(void* data, size_t length) {
        if (!isValid()) return false;

        char* buffer = static_cast<char*>(data);
        size_t totalBytesReceived = 0;

        while (totalBytesReceived < length) {
            int received = ::recv(m_handle, buffer + totalBytesReceived, static_cast<int>(length - totalBytesReceived), 0);
            if (received <= 0) {
                // Connection closed by peer or error
                return false;
            }
            totalBytesReceived += received;
        }

        return true;
    }

    void Socket::close() {
        if (m_handle != INVALID_SOCKET) {
#ifdef _WIN32
            ::closesocket(m_handle);
#else
            ::close(m_handle);
#endif
            m_handle = INVALID_SOCKET;
        }
    }

    bool Socket::isValid() const {
        return m_handle != INVALID_SOCKET;
    }

} // namespace fts
