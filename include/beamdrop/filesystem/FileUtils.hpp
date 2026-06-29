#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace beamdrop::filesystem {

[[nodiscard]] std::vector<std::uint8_t> read_file(const std::filesystem::path& path);
void write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes);

} // namespace beamdrop::filesystem
