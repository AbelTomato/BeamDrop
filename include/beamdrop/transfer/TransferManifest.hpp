#pragma once

#include <cstdint>
#include <vector>

namespace beamdrop::transfer {

struct TransferManifest {
    std::uint64_t file_count = 0;
    std::uint64_t total_bytes = 0;
};

class TransferManifestCodec {
public:
    static std::vector<std::uint8_t> encode(const TransferManifest& manifest);
    static TransferManifest decode(const std::vector<std::uint8_t>& payload);
};

} // namespace beamdrop::transfer