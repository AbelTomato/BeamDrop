#pragma once

#include "beamdrop/protocol/Packet.hpp"

#include <cstdint>
#include <span>

namespace beamdrop::protocol {

class PacketParser {
public:
    static Packet parse(std::span<const std::uint8_t> bytes);
};

} // namespace beamdrop::protocol
