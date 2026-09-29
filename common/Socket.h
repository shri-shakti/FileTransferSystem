#pragma once

#include <string>
#include <vector>
#include <cstdint>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET SocketHandle;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int SocketHandle;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR   -1
#endif

namespace fts {

    // Ensures Winsock library (WSAStartup/WSACleanup) is automatically initialized and closed
    class NetworkContext {
    public:
        static void initialize();
        static void cleanup();
    };

    /**
     * @brief RAII TCP Socket wrapper.
     * 
     * Core C++ Concepts showcased:
     * 1. RAII (Resource Acquisition Is Initialization): Destructor guarantees closesocket().
     * 2. Rule of 5: Copying is disabled (avoids double-close bugs). Moving is supported.
     * 3. Stream Handling: Provides sendAll() and receiveAll() to guarantee complete byte delivery.
     */
    class Socket {
    public:
        // Default constructor: creates an empty/uninitialized socket
        Socket();

        // Takes ownership of an existing raw socket descriptor
        explicit Socket(SocketHandle handle);

        // Destructor: Automatically closes the socket if it is open (RAII)
        ~Socket();

        // Disable Copying: Two objects cannot own the same socket descriptor!
        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;

        // Enable Moving: Transfer socket ownership safely
        Socket(Socket&& other) noexcept;
        Socket& operator=(Socket&& other) noexcept;

        // Server operations
        bool create();
        bool bindAndListen(int port, int backlog = SOMAXCONN);
        Socket accept(std::string& clientIp, uint16_t& clientPort);

        // Client operations
        bool connect(const std::string& host, int port);

        // Data Transmission (Handles TCP stream fragmentation)
        // Returns true if ALL bytes were transmitted, false on connection drop/error.
        bool sendAll(const void* data, size_t length);

        // Returns true if ALL bytes were received, false on disconnect/error.
        bool receiveAll(void* data, size_t length);

        // Close the socket manually (called automatically by destructor anyway)
        void close();

        // Check if the socket is valid and connected
        bool isValid() const;

        SocketHandle getHandle() const { return m_handle; }

    private:
        SocketHandle m_handle;
    };

} // namespace fts
