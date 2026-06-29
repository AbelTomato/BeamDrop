#pragma once

#include "beamdrop/network/TcpConnection.hpp"

#include <cstdint>
#include <string>

namespace beamdrop::network {

class TcpClient {
public:
    [[nodiscard]] TcpConnection connect(const std::string& host, std::uint16_t port) const;
};

} // namespace beamdrop::network
