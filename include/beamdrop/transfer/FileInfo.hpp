#pragma once

#include <cstdint>
#include <string>

namespace beamdrop::transfer {

struct FileInfo {
    std::string relative_path;
    std::uint64_t size{0};
    std::string sha256;
};

} // namespace beamdrop::transfer
