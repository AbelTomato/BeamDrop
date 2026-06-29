#pragma once

#include <cstdint>

namespace beamdrop::protocol {

enum class PacketType : std::uint16_t {
    Hello = 1,
    FileInfo = 2,
    ResumeReq = 3,
    ResumeAck = 4,
    Data = 5,
    FileEnd = 6,
    Finish = 7,
    Error = 8,
    Heartbeat = 9,
};

} // namespace beamdrop::protocol
