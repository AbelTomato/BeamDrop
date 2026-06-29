#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace beamdrop::utils {

class Sha256 {
public:
    Sha256();

    void update(std::span<const std::uint8_t> bytes);
    [[nodiscard]] std::string final_hex();

private:
    void process_block(std::span<const std::uint8_t, 64> block);

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_size_{0};
    std::uint64_t total_bytes_{0};
    bool finalized_{false};
};

[[nodiscard]] std::string sha256_hex(const std::vector<std::uint8_t>& bytes);
[[nodiscard]] std::string sha256_file(const std::filesystem::path& path, std::size_t chunk_size);

} // namespace beamdrop::utils
