# Modern C++ File Transfer System (Client-Server)

A high-performance, multi-threaded TCP File Transfer System built with **Modern C++17**, featuring an RAII networking engine, custom wire protocol, chunked streaming, concurrency synchronization, and an interactive command-line interface.

---

## Key Features

- **Multi-Threaded Architecture (`std::thread`)**: Spawns independent worker threads per client to serve concurrent uploads, downloads, and listings simultaneously.
- **Thread Safety & Race-Condition Prevention (`std::mutex`, `std::lock_guard`)**: Protects shared file storage and console logging from interleaved writes and race conditions.
- **Lock-Free Atomic Counters (`std::atomic<int>`)**: Real-time tracking of active connected clients with zero lock contention.
- **RAII-Managed Networking**: Custom TCP socket wrapper that guarantees automatic cleanup and zero resource/handle leaks (`closesocket`).
- **Custom Binary Wire Protocol**: Fixed-size 16-byte packed headers, magic bytes identification (`0x46545331` / "FTS1"), command dispatch, and network byte order (Big-Endian) translation.
- **Memory-Efficient Chunked Streaming**: Files are transferred in **64 KB chunks** (`BUFFER_CHUNK_SIZE`), keeping memory consumption constant $O(1)$ even when transferring multi-gigabyte files.
- **Directory Traversal Protection**: File path sanitization to block malicious requests (e.g., `../../`).
- **Interactive CLI**: Real-time terminal progress bar and interactive prompt (`list`, `upload`, `download`, `help`, `exit`).

---

## Wire Protocol Specification

Each communication packet starts with a fixed **16-byte packed header**:

```
+--------------------+--------------+---------------+-----------------------+---------------------+
| Magic (4 bytes)    | Command (1B) | Status (1B)   | Filename Length (2B)  | Payload Size (8B)   |
| 0x46545331 ("FTS1")| 1=LIST,etc.  | 0=OK, 1=ERROR | Length in bytes       | Total file size (B) |
+--------------------+--------------+---------------+-----------------------+---------------------+
```

Followed immediately by:
1. **Filename** (if `filenameLength > 0`)
2. **Payload / Binary Chunks** (if `payloadSize > 0`)

---

## Project Structure

```
FileTransferSystem/
├── CMakeLists.txt         # Modern target-based CMake configuration
├── build.ps1              # One-click Windows PowerShell build script
├── common/                # Shared library
│   ├── Protocol.h         # Wire protocol headers, enums, endian helpers
│   ├── Socket.h           # RAII Socket interface
│   └── Socket.cpp         # Socket implementation (sendAll, receiveAll)
├── server/                # Server Application
│   ├── Server.h           # Multi-threaded Server class interface
│   ├── Server.cpp         # Concurrency loop, thread dispatch, mutex sync
│   └── main.cpp           # Server entry point
├── client/                # Client Application
│   ├── Client.h           # Client class interface
│   ├── Client.cpp         # Network client with chunked streaming & progress bar
│   └── main.cpp           # Interactive CLI shell
└── storage/               # Server-side file repository
```

---

## How to Build and Run

### 1. Build using PowerShell
```powershell
.\build.ps1
```

### 2. Run the Server
```powershell
.\build\Debug\server.exe
```

### 3. Run the Client (in a separate terminal)
```powershell
.\build\Debug\client.exe
```

---

## Interactive Client Commands

```text
fts> list
--- Files on Server ---
sample.txt (130 bytes)
-----------------------

fts> upload path\to\my_file.pdf
[==============================] 100% (245012/245012 bytes)
[Client] SUCCESS: File 'my_file.pdf' uploaded successfully!

fts> download my_file.pdf my_local_copy.pdf
[==============================] 100% (245012/245012 bytes)
[Client] SUCCESS: Download complete! Saved to: my_local_copy.pdf

fts> exit
```

---

## Core C++ Concepts for Technical Interviews

1. **Multi-Threading (`std::thread`)**:
   - Why a single-threaded server blocks when one client uploads a 1GB file, and how spawning threads per client enables concurrent I/O throughput.
2. **Mutex Synchronization (`std::mutex`, `std::lock_guard`)**:
   - Preventing race conditions where two threads try to read/write the same file or modify directory structures at the same time.
   - Using RAII `std::lock_guard` to guarantee locks are released even if exceptions occur.
3. **Atomic Operations (`std::atomic`)**:
   - Tracking active client connections without lock overhead.
4. **RAII (Resource Acquisition Is Initialization)**:
   - Sockets and files automatically clean up via C++ destructors when going out of scope.
5. **Rule of Five**:
   - Explicitly deleted copy constructors and copy assignment operators prevent multiple instances from closing the same native socket descriptor.
6. **TCP Stream Buffering**:
   - Sockets do not transmit whole blocks in a single syscall. `sendAll` and `receiveAll` loop until byte delivery is complete.
7. **Binary vs Text Mode I/O**:
   - `std::ios::binary` prevents newline conversions (`\r\n` vs `\n`) from corrupting transferred binary files.
8. **Security (Path Traversal)**:
   - Stripping folder paths with `std::filesystem::path::filename()` ensures clients cannot write outside the designated server directory.
