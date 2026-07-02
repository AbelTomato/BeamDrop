#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace beamdrop::transfer {

struct ResumeRecord {
    std::string relative_path;
    std::uint64_t size{};
    std::string sha256;
    std::uint64_t offset{};
};

class ResumeManager {
public:
    explicit ResumeManager(std::filesystem::path state_file);

    [[nodiscard]] std::vector<ResumeRecord> load() const;
    void save(const std::vector<ResumeRecord>& records) const;

    [[nodiscard]] std::optional<std::uint64_t> find_offset(const std::string& relative_path,
                                                           std::uint64_t size,
                                                           const std::string& sha256) const;
    void update_offset(const std::string& relative_path,
                       std::uint64_t size,
                       const std::string& sha256,
                       std::uint64_t offset) const;
    void clear(const std::string& relative_path, std::uint64_t size, const std::string& sha256) const;

private:
    std::filesystem::path state_file_;
};

} // namespace beamdrop::transfer