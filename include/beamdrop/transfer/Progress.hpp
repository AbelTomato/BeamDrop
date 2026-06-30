#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace beamdrop::transfer {

enum class ProgressDirection {
    Send,
    Receive,
};

struct ProgressEvent {
    ProgressDirection direction = ProgressDirection::Send;
    std::string relative_path;
    std::uint64_t current_file_bytes = 0;
    std::uint64_t current_file_total_bytes = 0;
    std::size_t file_index = 0;
    std::size_t file_count = 0;
    bool file_complete = false;
};

using ProgressCallback = std::function<void(const ProgressEvent&)>;

} // namespace beamdrop::transfer