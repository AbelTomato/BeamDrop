#include "beamdrop/protocol/PacketParser.hpp"
#include "beamdrop/protocol/Serializer.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using beamdrop::protocol::Packet;
using beamdrop::protocol::PacketHeader;
using beamdrop::protocol::PacketParser;
using beamdrop::protocol::PacketType;
using beamdrop::protocol::Serializer;

namespace {

void test_encode_parse_roundtrip() {
    Packet packet;
    packet.header.type = PacketType::FileInfo;
    packet.header.flags = 42;
    packet.payload = {'{', '}', '\n'};
    packet.header.checksum[0] = 0xAB;

    const auto encoded = Serializer::encode(packet);
    assert(encoded.size() == PacketHeader::kEncodedSize + packet.payload.size());

    const auto parsed = PacketParser::parse(encoded);
    assert(parsed.header.magic == PacketHeader::kMagic);
    assert(parsed.header.version == PacketHeader::kVersion);
    assert(parsed.header.type == PacketType::FileInfo);
    assert(parsed.header.flags == 42);
    assert(parsed.header.length == packet.payload.size());
    assert(parsed.header.checksum[0] == 0xAB);
    assert(parsed.payload == packet.payload);
}

void test_reject_invalid_magic() {
    Packet packet;
    const auto encoded = Serializer::encode(packet);
    auto broken = encoded;
    broken[0] = 'X';

    bool rejected = false;
    try {
        (void)PacketParser::parse(broken);
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

void test_reject_length_mismatch() {
    Packet packet;
    packet.payload = {1, 2, 3};
    auto encoded = Serializer::encode(packet);
    encoded.pop_back();

    bool rejected = false;
    try {
        (void)PacketParser::parse(encoded);
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

} // namespace

int main() {
    test_encode_parse_roundtrip();
    test_reject_invalid_magic();
    test_reject_length_mismatch();

    std::cout << "protocol_tests passed\n";
    return 0;
}
