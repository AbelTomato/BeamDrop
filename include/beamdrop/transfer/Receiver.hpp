#pragma once

#include "beamdrop/network/TcpConnection.hpp"
#include "beamdrop/transfer/Progress.hpp"

#include <filesystem>
#include <cstddef>

namespace beamdrop::transfer {

class Receiver {
public:
    explicit Receiver(const network::TcpConnection& connection, ProgressCallback progress_callback = {});

    void receive_one_file(const std::filesystem::path& save_dir) const;
    void receive_files(const std::filesystem::path& save_dir, std::size_t file_count) const;

private:
    void receive_one_file(const std::filesystem::path& save_dir,
                          std::size_t file_index,
                          std::size_t file_count) const;

    const network::TcpConnection& connection_;
    ProgressCallback progress_callback_;
};

} // namespace beamdrop::transfer
