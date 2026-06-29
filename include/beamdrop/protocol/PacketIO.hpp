#pragma once

#include "beamdrop/network/TcpConnection.hpp"
#include "beamdrop/protocol/Packet.hpp"

namespace beamdrop::protocol {

[[nodiscard]] Packet read_packet(const network::TcpConnection& connection);
void write_packet(const network::TcpConnection& connection, const Packet& packet);

} // namespace beamdrop::protocol
