#pragma once

#include "beamdrop/app/TransferTask.hpp"
#include <cstdint>
#include <filesystem>
#include <stop_token>
#include <vector>

namespace beamdrop::app {
struct SendRequest {
    std::vector<std::filesystem::path> paths;
    std::string host;
    std::uint16_t port = 0;
    std::size_t chunk_size = 1024 * 1024;
    ProgressCallback progress_callback = {};
    std::stop_token stop_token = {};
};

struct SendResult {
    std::size_t file_count = 0;
    std::uint64_t total_bytes = 0;
};

class SendService {
  public:
    [[nodiscard]] ServiceResult<SendResult> send(const SendRequest &request) const;
};
} // namespace beamdrop::app