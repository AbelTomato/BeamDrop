#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace beamdrop::transfer {

class DirectoryInfoCodec {
  public:
    [[nodiscard]] static std::vector<std::uint8_t> encode(const std::string& relative_path);
    [[nodiscard]] static std::string decode(const std::vector<std::uint8_t>& payload);
};

} // namespace beamdrop::transfer