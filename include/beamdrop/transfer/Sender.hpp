#pragma once

#include "beamdrop/network/TcpConnection.hpp"
#include "beamdrop/filesystem/FileScanner.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace beamdrop::transfer {

class Sender {
public:
    explicit Sender(const network::TcpConnection& connection);

    void send_file(const std::filesystem::path& source_path, const std::string& relative_path) const;
    void send_files(const std::vector<filesystem::FileEntry>& entries) const;
    void send_path(const std::filesystem::path& input_path) const;

private:
    const network::TcpConnection& connection_;
};

} // namespace beamdrop::transfer
