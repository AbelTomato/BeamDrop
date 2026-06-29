#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <stdexcept>
#include <string>

namespace beamdrop::network::detail {

#ifdef _WIN32
using SocketHandle = SOCKET;
inline constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

class WinsockRuntime {
public:
    WinsockRuntime() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }

    ~WinsockRuntime() { WSACleanup(); }
};

inline void ensure_socket_runtime() {
    static WinsockRuntime runtime;
    (void)runtime;
}

inline void close_socket(SocketHandle handle) { closesocket(handle); }
inline int last_socket_error() { return WSAGetLastError(); }
#else
using SocketHandle = int;
inline constexpr SocketHandle kInvalidSocket = -1;

inline void ensure_socket_runtime() {}
inline void close_socket(SocketHandle handle) { close(handle); }
inline int last_socket_error() { return errno; }
#endif

inline SocketHandle to_socket_handle(std::intptr_t handle) {
#ifdef _WIN32
    return static_cast<SocketHandle>(handle);
#else
    return static_cast<SocketHandle>(handle);
#endif
}

inline std::intptr_t to_native_handle(SocketHandle handle) {
    return static_cast<std::intptr_t>(handle);
}

inline std::runtime_error socket_error(const std::string& message) {
    return std::runtime_error(message + ": socket error " + std::to_string(last_socket_error()));
}

} // namespace beamdrop::network::detail
