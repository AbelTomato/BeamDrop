#pragma once

#include "beamdrop/protocol/PacketType.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace beamdrop::protocol {

struct PacketHeader {
    static constexpr std::array<std::uint8_t, 4> kMagic{'B', 'D', 'R', 'P'};
    static constexpr std::uint16_t kVersion = 1;
    static constexpr std::size_t kEncodedSize = 52;

    std::array<std::uint8_t, 4> magic{kMagic};
    std::uint16_t version{kVersion};
    PacketType type{PacketType::Hello};
    std::uint32_t flags{0};
    std::uint64_t length{0};
    std::array<std::uint8_t, 32> checksum{};
};

} // namespace beamdrop::protocol
