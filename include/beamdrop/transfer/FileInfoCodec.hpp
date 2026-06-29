#pragma once

#include "beamdrop/transfer/FileInfo.hpp"

#include <cstdint>
#include <vector>

namespace beamdrop::transfer {

class FileInfoCodec {
public:
    [[nodiscard]] static std::vector<std::uint8_t> encode(const FileInfo& info);
    [[nodiscard]] static FileInfo decode(const std::vector<std::uint8_t>& payload);
};

} // namespace beamdrop::transfer
