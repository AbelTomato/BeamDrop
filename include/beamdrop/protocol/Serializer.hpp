#pragma once

#include "beamdrop/protocol/Packet.hpp"

#include <cstdint>
#include <vector>

namespace beamdrop::protocol {

class Serializer {
public:
    static std::vector<std::uint8_t> encode(const Packet& packet);
};

} // namespace beamdrop::protocol
