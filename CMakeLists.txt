cmake_minimum_required(VERSION 3.15)

# -------------------------------------------------------------
# 1. Project Information
# -------------------------------------------------------------
project(FileTransferSystem 
    VERSION 1.0.0 
    DESCRIPTION "High-performance C++ Client-Server File Transfer System"
    LANGUAGES CXX
)

# -------------------------------------------------------------
# 2. Modern C++ Standard Configuration
# -------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------------------------------------------------
# 3. Compiler Warning Flags
# -------------------------------------------------------------
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# -------------------------------------------------------------
# 4. Platform-Specific Networking Dependencies
# -------------------------------------------------------------
if(WIN32)
    set(NETWORKING_LIBS ws2_32)
else()
    set(NETWORKING_LIBS "")
endif()

# -------------------------------------------------------------
# 5. 'common' Static Library
# -------------------------------------------------------------
add_library(common STATIC
    common/Socket.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

target_link_libraries(common PUBLIC
    ${NETWORKING_LIBS}
)

# -------------------------------------------------------------
# 6. Server Executable
# -------------------------------------------------------------
add_executable(server
    server/Server.cpp
    server/main.cpp
)

target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server
)

target_link_libraries(server PRIVATE
    common
)

# -------------------------------------------------------------
# 7. Client Executable
# -------------------------------------------------------------
add_executable(client
    client/Client.cpp
    client/main.cpp
)

target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/client
)

target_link_libraries(client PRIVATE
    common
)
