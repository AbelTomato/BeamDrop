#include "beamdrop/transfer/ResumeAckCodec.hpp"

#include <cstddef>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace beamdrop::transfer {

std::vector<std::uint8_t> ResumeAckCodec::encode(const ResumeAck& ack) {
    std::ostringstream stream;
    stream << "{\"offset\":" << ack.offset << "}";
    const auto text = stream.str();
    return {text.begin(), text.end()};
}

ResumeAck ResumeAckCodec::decode(const std::vector<std::uint8_t>& payload) {
    const std::string text{payload.begin(), payload.end()};
    const std::regex pattern{"^\\s*\\{\\s*\\\"offset\\\"\\s*:\\s*([0-9]+)\\s*\\}\\s*$"};
    std::smatch match;
    if (!std::regex_match(text, match, pattern)) {
        throw std::runtime_error("invalid RESUME_ACK payload");
    }

    try {
        std::size_t parsed_chars = 0;
        const auto value = std::stoull(match[1].str(), &parsed_chars);
        if (parsed_chars != match[1].str().size()) {
            throw std::runtime_error("invalid RESUME_ACK offset");
        }
        return ResumeAck{static_cast<std::uint64_t>(value)};
    } catch (const std::exception&) {
        throw std::runtime_error("invalid RESUME_ACK offset");
    }
}

} // namespace beamdrop::transfer