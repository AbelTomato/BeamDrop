#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace beamdrop::network {
enum class ErrorCode {
    RuntimeInitializationFailed,
    AddressResolutionFailed,
    SocketCreationFailed,
    SetSocketOptionFailed,
    BindFailed,
    ListenFailed,
    AcceptFailed,
    ConnectFailed,
    SendFailed,
    ReceiveFailed,
    InvalidAddress
};

class NetworkError : public std::runtime_error {
  public:
    NetworkError(ErrorCode code, std::string message, int native_error = 0, std::string host = {},
                 std::uint16_t port = 0)
        : std::runtime_error(std::move(message)), code_(code), native_error_(native_error),
          host_(std::move(host)), port_(port) {}

    [[nodiscard]] ErrorCode code() const noexcept {
        return code_;
    }

    [[nodiscard]] int native_error() const noexcept {
        return native_error_;
    }

    [[nodiscard]] const std::string &host() const noexcept {
        return host_;
    }

    [[nodiscard]] std::uint16_t port() const noexcept {
        return port_;
    }

  private:
    ErrorCode code_;
    int native_error_;
    std::string host_;
    std::uint16_t port_;
};
} // namespace beamdrop::network