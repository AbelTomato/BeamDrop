#include "beamdrop/protocol/PacketIO.hpp"

#include "beamdrop/protocol/PacketHeader.hpp"
#include "beamdrop/protocol/PacketParser.hpp"
#include "beamdrop/protocol/Serializer.hpp"

#include <cstdint>

namespace beamdrop::protocol {

Packet read_packet(const network::TcpConnection& connection) {
    auto bytes = connection.read_exact(PacketHeader::kEncodedSize);

    std::uint64_t payload_length = 0;
    constexpr std::size_t length_offset = 4 + 2 + 2 + 4;
    for (std::size_t i = 0; i < 8; ++i) {
        payload_length = (payload_length << 8) | bytes[length_offset + i];
    }

    if (payload_length > 0) {
        auto payload = connection.read_exact(static_cast<std::size_t>(payload_length));
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    }

    return PacketParser::parse(bytes);
}

void write_packet(const network::TcpConnection& connection, const Packet& packet) {
    connection.write_all(Serializer::encode(packet));
}

} // namespace beamdrop::protocol
