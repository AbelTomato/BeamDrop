#pragma once

#include "beamdrop/network/TcpConnection.hpp"

#include <cstdint>
#include <string>

namespace beamdrop::network {

class TcpServer {
public:
    TcpServer(const std::string& host, std::uint16_t port);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    [[nodiscard]] TcpConnection accept_one() const;
    void close() noexcept;

private:
    TcpConnection::NativeHandle listen_handle_{-1};
};

} // namespace beamdrop::network
