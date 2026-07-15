#pragma once

#include "beamdrop/network/TcpConnection.hpp"
#include "beamdrop/transfer/Progress.hpp"

#include <cstddef>
#include <filesystem>

namespace beamdrop::transfer {

class Receiver {
public:
    explicit Receiver(const network::TcpConnection& connection,
                      ProgressCallback progress_callback = {},
                      bool enable_resume = false,
                      std::filesystem::path state_file = "transfer_state.json");

    void receive_file(const std::filesystem::path& save_dir) const;
    void receive_task(const std::filesystem::path& save_dir, std::size_t file_count) const;

private:
    void receive_one_file(const std::filesystem::path& save_dir,
                          std::size_t file_index,
                          std::size_t file_count) const;

    const network::TcpConnection& connection_;
    ProgressCallback progress_callback_;
    bool enable_resume_;
    std::filesystem::path state_file_;
};

} // namespace beamdrop::transfer
