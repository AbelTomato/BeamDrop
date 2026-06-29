#pragma once

#include "beamdrop/protocol/PacketHeader.hpp"

#include <cstdint>
#include <vector>

namespace beamdrop::protocol {

struct Packet {
    PacketHeader header{};
    std::vector<std::uint8_t> payload{};
};

} // namespace beamdrop::protocol
