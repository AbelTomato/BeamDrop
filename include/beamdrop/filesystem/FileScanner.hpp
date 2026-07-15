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

struct DirectoryEntry {
    std::filesystem::path source_path;
    std::string relative_path;
};

std::vector<FileEntry> scan_files(const std::filesystem::path& input_path);
std::vector<DirectoryEntry> scan_directories(const std::filesystem::path& input_path);

} // namespace beamdrop::filesystem