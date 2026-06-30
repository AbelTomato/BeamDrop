#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace beamdrop::config {

struct ServerConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 9090;
    std::filesystem::path save_dir = "received";
};

struct TransferConfig {
    std::size_t chunk_size = 1024 * 1024;
    bool enable_resume = false;
    bool verify_sha256 = true;
    std::filesystem::path state_file = "transfer_state.json";
};

struct LogConfig {
    std::string level = "info";
    std::filesystem::path file = "logs/beamdrop.log";
};

struct AppConfig {
    ServerConfig server;
    TransferConfig transfer;
    LogConfig log;
};

[[nodiscard]] AppConfig default_config();
[[nodiscard]] AppConfig load_config(const std::filesystem::path& path);
[[nodiscard]] std::string format_config(const AppConfig& config);
void write_config(const std::filesystem::path& path, const AppConfig& config);

} // namespace beamdrop::config