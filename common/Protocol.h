#pragma once

#include <cstdint>
#include <string>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

namespace fts {

    // Magic identifier: "FTS1" (0x46545331 in Hex).
    // This allows the server to verify that the incoming data is indeed from our application.
    constexpr uint32_t PROTOCOL_MAGIC = 0x46545331;

    // Default port for our server
    constexpr int DEFAULT_PORT = 8080;

    // Buffer size used for chunked file streaming (64 Kilobytes).
    // Large files are transferred piece-by-piece in 64 KB chunks, keeping RAM usage low.
    constexpr size_t BUFFER_CHUNK_SIZE = 64 * 1024;

    // Supported commands between Client and Server
    enum class Command : uint8_t {
        LIST_FILES   = 1,   // Ask server to list available files
        UPLOAD_FILE  = 2,   // Send a file from client to server
        DOWNLOAD_FILE= 3,   // Download a file from server to client
        DISCONNECT   = 4    // Gracefully terminate the connection
    };

    // Status responses returned by Server to Client
    enum class StatusCode : uint8_t {
        OK               = 0,  // Success
        FILE_NOT_FOUND   = 1,  // Requested file does not exist
        FILE_ALREADY_EXISTS = 2,
        SERVER_ERROR     = 3,  // Disk full, I/O failure, etc.
        INVALID_PACKET   = 4   // Corrupt or unrecognized protocol header
    };

    // Packet Header
    // #pragma pack(push, 1) instructs the compiler to pack struct fields tightly
    // with NO padding bytes in between. This guarantees identical binary layout
    // across different compilers and platforms.
    #pragma pack(push, 1)
    struct PacketHeader {
        uint32_t magic;          // 4 bytes: Must match PROTOCOL_MAGIC
        uint8_t  command;        // 1 byte:  Command type (Command enum)
        uint8_t  status;         // 1 byte:  Status code (StatusCode enum)
        uint16_t filenameLength; // 2 bytes: Length of the filename string (0 if none)
        uint64_t payloadSize;    // 8 bytes: Total byte size of the file/data payload
    };
    #pragma pack(pop)

    // Helper functions for Network Byte Order (Big-Endian vs Little-Endian)
    // Most CPUs (Intel/AMD/ARM) are Little-Endian, but network protocols standardly use Big-Endian.
    inline uint64_t hostToNetwork64(uint64_t value) {
#ifdef _WIN32
        return htonll(value);
#else
        return (((uint64_t)htonl((uint32_t)(value & 0xFFFFFFFF))) << 32) |
                (uint64_t)htonl((uint32_t)(value >> 32));
#endif
    }

    inline uint64_t networkToHost64(uint64_t value) {
#ifdef _WIN32
        return ntohll(value);
#else
        return hostToNetwork64(value);
#endif
    }

    // Convert PacketHeader to Network Byte Order before sending
    inline PacketHeader serializeHeader(PacketHeader hdr) {
        hdr.magic = htonl(hdr.magic);
        hdr.filenameLength = htons(hdr.filenameLength);
        hdr.payloadSize = hostToNetwork64(hdr.payloadSize);
        return hdr;
    }

    // Convert PacketHeader from Network Byte Order to Host Byte Order after receiving
    inline PacketHeader deserializeHeader(PacketHeader hdr) {
        hdr.magic = ntohl(hdr.magic);
        hdr.filenameLength = ntohs(hdr.filenameLength);
        hdr.payloadSize = networkToHost64(hdr.payloadSize);
        return hdr;
    }

} // namespace fts
