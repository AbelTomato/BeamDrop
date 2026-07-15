#include "beamdrop/network/TcpConnection.hpp"
#include <climits>

#include "SocketPlatform.hpp"
#include "beamdrop/network/NetworkError.hpp"

namespace beamdrop::network {

TcpConnection::TcpConnection(NativeHandle handle) : handle_(handle) {}

TcpConnection::~TcpConnection() {
    close();
}

TcpConnection::TcpConnection(TcpConnection &&other) noexcept
    : handle_(other.handle_.exchange(-1)) {
}

TcpConnection &TcpConnection::operator=(TcpConnection &&other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_.exchange(-1);
    }
    return *this;
}

bool TcpConnection::valid() const noexcept {
    return handle_.load() != -1;
}

void TcpConnection::shutdown() noexcept {
    const auto handle = handle_.exchange(-1);
    if (handle == -1) {
        return;
    }

#ifdef _WIN32
    ::shutdown(detail::to_socket_handle(handle), SD_BOTH);
#else
    ::shutdown(detail::to_socket_handle(handle), SHUT_RDWR);
#endif
    detail::close_socket(detail::to_socket_handle(handle));
}

void TcpConnection::close() noexcept {
    const auto handle = handle_.exchange(-1);
    if (handle == -1) {
        return;
    }

    detail::close_socket(detail::to_socket_handle(handle));
}

void TcpConnection::write_all(std::span<const std::uint8_t> bytes) const {
    const auto handle = handle_.load();
    if (handle == -1) {
        throw NetworkError{ErrorCode::ConnectFailed, "cannot write to invalid TCP connection"};
    }

    std::size_t sent_total = 0;
    while (sent_total < bytes.size()) {
        const auto remaining = bytes.size() - sent_total;
        const int chunk_size =
            remaining > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(remaining);
        const int sent =
            send(detail::to_socket_handle(handle),
                 reinterpret_cast<const char *>(bytes.data() + sent_total), chunk_size, 0);
        if (sent <= 0) {
            throw detail::socket_error(ErrorCode::SendFailed, "send failed");
        }
        sent_total += static_cast<std::size_t>(sent);
    }
}

std::vector<std::uint8_t> TcpConnection::read_exact(std::size_t size) const {
    const auto handle = handle_.load();
    if (handle == -1) {
        throw NetworkError{ErrorCode::ConnectFailed, "cannot read from invalid TCP connection"};
    }

    std::vector<std::uint8_t> bytes(size);
    std::size_t received_total = 0;
    while (received_total < size) {
        const auto remaining = size - received_total;
        const int chunk_size =
            remaining > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(remaining);
        const int received =
            recv(detail::to_socket_handle(handle),
                 reinterpret_cast<char *>(bytes.data() + received_total), chunk_size, 0);
        if (received <= 0) {
            throw detail::socket_error(ErrorCode::ReceiveFailed, "recv failed");
        }
        received_total += static_cast<std::size_t>(received);
    }

    return bytes;
}

} // namespace beamdrop::network
