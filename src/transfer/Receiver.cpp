#include "beamdrop/transfer/Receiver.hpp"

#include "beamdrop/filesystem/FileUtils.hpp"
#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/FileInfoCodec.hpp"
#include "beamdrop/utils/Sha256.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace beamdrop::transfer {
namespace {

std::filesystem::path safe_join(const std::filesystem::path& save_dir,
                                const std::string& relative_path) {
    const std::filesystem::path path{relative_path};
    if (path.is_absolute()) {
        throw std::runtime_error("absolute receive path is not allowed");
    }

    for (const auto& part : path) {
        if (part == "..") {
            throw std::runtime_error("path traversal is not allowed");
        }
    }

    return save_dir / path;
}

} // namespace

Receiver::Receiver(const network::TcpConnection& connection) : connection_(connection) {}

void Receiver::receive_one_file(const std::filesystem::path& save_dir) const {
    const auto info_packet = protocol::read_packet(connection_);
    if (info_packet.header.type != protocol::PacketType::FileInfo) {
        throw std::runtime_error("expected FILE_INFO packet");
    }

    const auto file_info = FileInfoCodec::decode(info_packet.payload);
    const auto output_path = safe_join(save_dir, file_info.relative_path);
    const auto parent = output_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to open output file: " + output_path.string());
    }

    utils::Sha256 sha256;
    std::uint64_t received_size = 0;

    while (true) {
        const auto packet = protocol::read_packet(connection_);
        if (packet.header.type == protocol::PacketType::FileEnd) {
            break;
        }
        if (packet.header.type != protocol::PacketType::Data) {
            throw std::runtime_error("expected DATA or FILE_END packet");
        }

        received_size += packet.payload.size();
        if (received_size > file_info.size) {
            throw std::runtime_error("received data size exceeds FILE_INFO size");
        }

        sha256.update(packet.payload);
        if (!packet.payload.empty()) {
            output.write(reinterpret_cast<const char*>(packet.payload.data()),
                         static_cast<std::streamsize>(packet.payload.size()));
            if (!output) {
                throw std::runtime_error("failed to write output file: " + output_path.string());
            }
        }
    }

    output.close();
    if (!output) {
        throw std::runtime_error("failed to close output file: " + output_path.string());
    }

    if (received_size != file_info.size) {
        throw std::runtime_error("received data size mismatch");
    }
    if (sha256.final_hex() != file_info.sha256) {
        throw std::runtime_error("received data sha256 mismatch");
    }
}

void Receiver::receive_files(const std::filesystem::path& save_dir, std::size_t file_count) const {
    for (std::size_t index = 0; index < file_count; ++index) {
        receive_one_file(save_dir);
    }
}

} // namespace beamdrop::transfer
