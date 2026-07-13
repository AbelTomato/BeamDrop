#include "beamdrop/network/TcpServer.hpp"

#include "SocketPlatform.hpp"
#include "beamdrop/network/NetworkError.hpp"
#include "beamdrop/network/TcpConnection.hpp"

#include <optional>
#include <stdexcept>

namespace beamdrop::network {

TcpServer::TcpServer(const std::string &host, std::uint16_t port) {
    detail::ensure_socket_runtime();

    const auto handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (handle == detail::kInvalidSocket) {
        throw detail::socket_error(ErrorCode::SocketCreationFailed, "socket creation failed");
    }

    int yes = 1;
    if (setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes),
                   sizeof(yes)) != 0) {
        detail::close_socket(handle);
        throw detail::socket_error(ErrorCode::SetSocketOptionFailed, "set socket REUSEADDR failed");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        detail::close_socket(handle);
        throw NetworkError{ErrorCode::InvalidAddress, "invalid bind host" + host};
    }

    if (bind(handle, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        detail::close_socket(handle);
        throw detail::socket_error(ErrorCode::BindFailed, "bind failed");
    }

    if (listen(handle, 8) != 0) {
        detail::close_socket(handle);
        throw detail::socket_error(ErrorCode::ListenFailed, "listen failed");
    }

    listen_handle_ = detail::to_native_handle(handle);
}

TcpServer::~TcpServer() {
    close();
}

std::optional<TcpConnection> TcpServer::accept_one_for(std::chrono::milliseconds timeout) const {
    if (listen_handle_ == -1) {
        throw NetworkError{ErrorCode::AcceptFailed, "cannot accept on closed TCP server"};
    }

    if (timeout.count() < 0) {
        throw NetworkError{ErrorCode::AcceptFailed, "accept timeout must not be negative"};
    }

    const auto socket = detail::to_socket_handle(listen_handle_);

    fd_set read_set{};
    FD_ZERO(&read_set);
    FD_SET(socket, &read_set);

    const auto milliseconds = timeout.count();
    constexpr auto kMillisecondsPerSecond = 1000;
    constexpr auto kMicrosecondsPerMillisecond = 1000;

    timeval wait_time{};
    wait_time.tv_sec = static_cast<long>(milliseconds / kMillisecondsPerSecond);
    wait_time.tv_usec =
        static_cast<long>((milliseconds % kMillisecondsPerSecond) * kMicrosecondsPerMillisecond);

#ifdef _WIN32
    const int ready = select(0, &read_set, nullptr, nullptr, &wait_time);
#else
    const int ready = select(static_cast<int>(socket) + 1, &read_set, nullptr, nullptr, &wait_time);
#endif

    if (ready == 0) {
        return std::nullopt;
    }

    if (ready < 0) {
        throw detail::socket_error(ErrorCode::AcceptFailed, "wait for incoming connection failed");
    }

    return accept_one();
}

TcpConnection TcpServer::accept_one() const {
    if (listen_handle_ == -1) {
        throw NetworkError{ErrorCode::AcceptFailed, "cannot accept on closed TCP server"};
    }

    sockaddr_in client_address{};
#ifdef _WIN32
    int address_length = sizeof(client_address);
#else
    socklen_t address_length = sizeof(client_address);
#endif
    const auto client = accept(detail::to_socket_handle(listen_handle_),
                               reinterpret_cast<sockaddr *>(&client_address), &address_length);
    if (client == detail::kInvalidSocket) {
        throw detail::socket_error(ErrorCode::AcceptFailed, "accept failed");
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
