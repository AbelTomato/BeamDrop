#pragma once

#include "beamdrop/network/TcpConnection.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace beamdrop::network {

class TcpServer {
  public:
    TcpServer(const std::string &host, std::uint16_t port);
    ~TcpServer();

    TcpServer(const TcpServer &) = delete;
    TcpServer &operator=(const TcpServer &) = delete;

    [[nodiscard]] TcpConnection accept_one() const;

    [[nodiscard]] std::optional<TcpConnection>
    accept_one_for(std::chrono::milliseconds timeout) const;

    void close() noexcept;

  private:
    TcpConnection::NativeHandle listen_handle_{-1};
};

} // namespace beamdrop::network
