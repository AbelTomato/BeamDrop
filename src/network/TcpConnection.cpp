#include "beamdrop/network/TcpConnection.hpp"
#include <climits>

#include "SocketPlatform.hpp"

#include <stdexcept>
#include <utility>

namespace beamdrop::network {

TcpConnection::TcpConnection(NativeHandle handle) : handle_(handle) {}

TcpConnection::~TcpConnection() { close(); }

TcpConnection::TcpConnection(TcpConnection&& other) noexcept : handle_(other.handle_) {
    other.handle_ = -1;
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        other.handle_ = -1;
    }
    return *this;
}

bool TcpConnection::valid() const noexcept { return handle_ != -1; }

void TcpConnection::close() noexcept {
    if (!valid()) {
        return;
    }

    detail::close_socket(detail::to_socket_handle(handle_));
    handle_ = -1;
}

void TcpConnection::write_all(std::span<const std::uint8_t> bytes) const {
    if (!valid()) {
        throw std::runtime_error("cannot write to invalid TCP connection");
    }

    std::size_t sent_total = 0;
    while (sent_total < bytes.size()) {
        const auto remaining = bytes.size() - sent_total;
        const int chunk_size = remaining > static_cast<std::size_t>(INT_MAX)
                                   ? INT_MAX
                                   : static_cast<int>(remaining);
        const int sent = send(detail::to_socket_handle(handle_),
                              reinterpret_cast<const char*>(bytes.data() + sent_total), chunk_size, 0);
        if (sent <= 0) {
            throw detail::socket_error("send failed");
        }
        sent_total += static_cast<std::size_t>(sent);
    }
}

std::vector<std::uint8_t> TcpConnection::read_exact(std::size_t size) const {
    if (!valid()) {
        throw std::runtime_error("cannot read from invalid TCP connection");
    }

    std::vector<std::uint8_t> bytes(size);
    std::size_t received_total = 0;
    while (received_total < size) {
        const auto remaining = size - received_total;
        const int chunk_size = remaining > static_cast<std::size_t>(INT_MAX)
                                   ? INT_MAX
                                   : static_cast<int>(remaining);
        const int received = recv(detail::to_socket_handle(handle_),
                                  reinterpret_cast<char*>(bytes.data() + received_total), chunk_size, 0);
        if (received <= 0) {
            throw detail::socket_error("recv failed");
        }
        received_total += static_cast<std::size_t>(received);
    }

    return bytes;
}

} // namespace beamdrop::network
