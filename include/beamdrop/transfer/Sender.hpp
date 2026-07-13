#pragma once

#include "beamdrop/network/TcpConnection.hpp"
#include "beamdrop/filesystem/FileScanner.hpp"
#include "beamdrop/transfer/Progress.hpp"

#include <filesystem>
#include <cstddef>
#include <string>
#include <vector>

namespace beamdrop::transfer {

class Sender {
public:
    explicit Sender(const network::TcpConnection& connection,
                    ProgressCallback progress_callback = {},
                    std::size_t chunk_size = 1024 * 1024);

    void send_file(const std::filesystem::path& source_path, const std::string& relative_path) const;
    void send_task(const std::vector<filesystem::FileEntry>& entries) const;
    void send_path(const std::filesystem::path& input_path) const;

private:
    void send_one_file(const std::filesystem::path& source_path,
                       const std::string& relative_path,
                       std::size_t file_index,
                       std::size_t file_count) const;

    const network::TcpConnection& connection_;
    ProgressCallback progress_callback_;
    std::size_t chunk_size_;
};

} // namespace beamdrop::transfer
