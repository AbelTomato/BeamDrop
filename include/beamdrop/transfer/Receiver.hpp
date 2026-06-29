#pragma once

#include "beamdrop/network/TcpConnection.hpp"

#include <filesystem>
#include <cstddef>

namespace beamdrop::transfer {

class Receiver {
public:
    explicit Receiver(const network::TcpConnection& connection);

    void receive_one_file(const std::filesystem::path& save_dir) const;
    void receive_files(const std::filesystem::path& save_dir, std::size_t file_count) const;

private:
    const network::TcpConnection& connection_;
};

} // namespace beamdrop::transfer
