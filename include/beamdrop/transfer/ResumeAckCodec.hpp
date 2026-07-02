#pragma once

#include <cstdint>
#include <vector>

namespace beamdrop::transfer {

struct ResumeAck {
    std::uint64_t offset{};
};

class ResumeAckCodec {
public:
    [[nodiscard]] static std::vector<std::uint8_t> encode(const ResumeAck& ack);
    [[nodiscard]] static ResumeAck decode(const std::vector<std::uint8_t>& payload);
};

} // namespace beamdrop::transfer