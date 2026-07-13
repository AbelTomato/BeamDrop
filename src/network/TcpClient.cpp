#include "beamdrop/network/TcpClient.hpp"
#include "SocketPlatform.hpp"
#include "beamdrop/network/NetworkError.hpp"

namespace beamdrop::network {

TcpConnection TcpClient::connect(const std::string &host, std::uint16_t port) const {
    detail::ensure_socket_runtime();

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *result = nullptr;
    const auto port_text = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &result) != 0) {
        throw NetworkError{ErrorCode::AddressResolutionFailed, host, port,
                           "getaddrinfo failed for host "};
    }

    detail::SocketHandle connected = detail::kInvalidSocket;
    for (addrinfo *item = result; item != nullptr; item = item->ai_next) {
        const auto handle = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (handle == detail::kInvalidSocket) {
            continue;
        }

        if (::connect(handle, item->ai_addr, static_cast<int>(item->ai_addrlen)) == 0) {
            connected = handle;
            break;
        }

        detail::close_socket(handle);
    }

    freeaddrinfo(result);

    if (connected == detail::kInvalidSocket) {
        throw detail::socket_error(ErrorCode::ConnectFailed, "connect failed", host, port);
    }

    return TcpConnection{detail::to_native_handle(connected)};
}

} // namespace beamdrop::network
