#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace beamdrop::filesystem {

struct FileEntry {
    std::filesystem::path source_path;
    std::string relative_path;
    std::uintmax_t size;
};

std::vector<FileEntry> scan_files(const std::filesystem::path& input_path);

} // namespace beamdrop::filesystem