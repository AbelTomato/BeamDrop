#include "beamdrop/protocol/Serializer.hpp"

namespace beamdrop::protocol {
namespace {

void append_u16(std::vector<std::uint8_t> &out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
    }
}

void append_u64(std::vector<std::uint8_t> &out, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
    }
}

} // namespace

std::vector<std::uint8_t> Serializer::encode(const Packet &packet) {
    PacketHeader header = packet.header;
    header.length = packet.payload.size();

    std::vector<std::uint8_t> out;
    out.reserve(PacketHeader::kEncodedSize + packet.payload.size());

    out.insert(out.end(), header.magic.begin(), header.magic.end());
    append_u16(out, header.version);
    append_u16(out, static_cast<std::uint16_t>(header.type));
    append_u32(out, header.flags);
    append_u64(out, header.length);
    out.insert(out.end(), header.checksum.begin(), header.checksum.end());
    out.insert(out.end(), packet.payload.begin(), packet.payload.end());

    return out;
}

} // namespace beamdrop::protocol
