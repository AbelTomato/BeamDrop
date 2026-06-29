#include "beamdrop/network/TcpServer.hpp"

#include "SocketPlatform.hpp"

#include <stdexcept>

namespace beamdrop::network {

TcpServer::TcpServer(const std::string& host, std::uint16_t port) {
    detail::ensure_socket_runtime();

    const auto handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (handle == detail::kInvalidSocket) {
        throw detail::socket_error("socket creation failed");
    }

    int yes = 1;
    setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        detail::close_socket(handle);
        throw std::runtime_error("invalid bind host: " + host);
    }

    if (bind(handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        detail::close_socket(handle);
        throw detail::socket_error("bind failed");
    }

    if (listen(handle, 8) != 0) {
        detail::close_socket(handle);
        throw detail::socket_error("listen failed");
    }

    listen_handle_ = detail::to_native_handle(handle);
}

TcpServer::~TcpServer() { close(); }

TcpConnection TcpServer::accept_one() const {
    if (listen_handle_ == -1) {
        throw std::runtime_error("cannot accept on closed TCP server");
    }

    sockaddr_in client_address{};
#ifdef _WIN32
    int address_length = sizeof(client_address);
#else
    socklen_t address_length = sizeof(client_address);
#endif
    const auto client = accept(detail::to_socket_handle(listen_handle_),
                               reinterpret_cast<sockaddr*>(&client_address), &address_length);
    if (client == detail::kInvalidSocket) {
        throw detail::socket_error("accept failed");
    }

    return TcpConnection{detail::to_native_handle(client)};
}

void TcpServer::close() noexcept {
    if (listen_handle_ == -1) {
        return;
    }

    detail::close_socket(detail::to_socket_handle(listen_handle_));
    listen_handle_ = -1;
}

} // namespace beamdrop::network
