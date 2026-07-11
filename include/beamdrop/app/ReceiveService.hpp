#pragma once

#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/network/TcpConnection.hpp"
#include <cstddef>
#include <filesystem>
#include <stop_token>

namespace beamdrop::app {
struct ReceiveRequest {
    ProgressCallback progress_callback = {};
    std::stop_token stop_token = {};
    bool enable_resume = false;
    std::filesystem::path state_file = {};
    std::filesystem::path save_dir = {};
};

struct ReceiveResult {
    std::size_t file_count = 0;
};

class ReceiveService {
  public:
    [[nodiscard]] ReceiveResult receive(const beamdrop::network::TcpConnection& connection, const ReceiveRequest& request) const;
};
}