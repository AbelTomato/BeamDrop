#include "beamdrop/protocol/PacketParser.hpp"

#include <stdexcept>

namespace beamdrop::protocol {
namespace {

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8) |
                                      static_cast<std::uint16_t>(bytes[offset + 1]));
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value = (value << 8) | bytes[offset + i];
    }
    return value;
}

std::uint64_t read_u64(std::span<const std::uint8_t> bytes, std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value = (value << 8) | bytes[offset + i];
    }
    return value;
}

PacketType parse_type(std::uint16_t value) {
    switch (static_cast<PacketType>(value)) {
    case PacketType::Hello:
    case PacketType::FileInfo:
    case PacketType::ResumeReq:
    case PacketType::ResumeAck:
    case PacketType::Data:
    case PacketType::FileEnd:
    case PacketType::Finish:
    case PacketType::Error:
    case PacketType::Heartbeat:
        return static_cast<PacketType>(value);
    }

    throw std::runtime_error("invalid packet type");
}

} // namespace

Packet PacketParser::parse(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < PacketHeader::kEncodedSize) {
        throw std::runtime_error("packet too short");
    }

    Packet packet;
    std::size_t offset = 0;

    for (std::size_t i = 0; i < packet.header.magic.size(); ++i) {
        packet.header.magic[i] = bytes[offset++];
    }

    if (packet.header.magic != PacketHeader::kMagic) {
        throw std::runtime_error("invalid packet magic");
    }

    packet.header.version = read_u16(bytes, offset);
    offset += 2;
    if (packet.header.version != PacketHeader::kVersion) {
        throw std::runtime_error("unsupported packet version");
    }

    packet.header.type = parse_type(read_u16(bytes, offset));
    offset += 2;
    packet.header.flags = read_u32(bytes, offset);
    offset += 4;
    packet.header.length = read_u64(bytes, offset);
    offset += 8;

    for (std::size_t i = 0; i < packet.header.checksum.size(); ++i) {
        packet.header.checksum[i] = bytes[offset++];
    }

    if (bytes.size() != PacketHeader::kEncodedSize + packet.header.length) {
        throw std::runtime_error("packet length mismatch");
    }

    packet.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
    return packet;
}

} // namespace beamdrop::protocol
