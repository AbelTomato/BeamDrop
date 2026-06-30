#include "beamdrop/transfer/Sender.hpp"

#include "beamdrop/protocol/Packet.hpp"
#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/FileInfoCodec.hpp"
#include "beamdrop/utils/Sha256.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace beamdrop::transfer {

Sender::Sender(const network::TcpConnection& connection,
               ProgressCallback progress_callback,
               std::size_t chunk_size)
    : connection_(connection), progress_callback_(std::move(progress_callback)), chunk_size_(chunk_size) {
    if (chunk_size_ == 0) {
        throw std::runtime_error("sender chunk_size must be greater than zero");
    }
}

void Sender::send_file(const std::filesystem::path& source_path,
                       const std::string& relative_path) const {
    send_file(source_path, relative_path, 1, 1);
}

void Sender::send_file(const std::filesystem::path& source_path,
                       const std::string& relative_path,
                       std::size_t file_index,
                       std::size_t file_count) const {
    const auto file_size = std::filesystem::file_size(source_path);
    const auto file_hash = utils::sha256_file(source_path, chunk_size_);

    protocol::Packet info_packet;
    info_packet.header.type = protocol::PacketType::FileInfo;
    info_packet.payload = FileInfoCodec::encode(FileInfo{relative_path, file_size, file_hash});
    protocol::write_packet(connection_, info_packet);

    std::ifstream input(source_path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open input file for streaming: " + source_path.string());
    }

    std::vector<std::uint8_t> buffer(chunk_size_);
    std::uint64_t sent_size = 0;
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const auto read_count = input.gcount();
        if (read_count <= 0) {
            break;
        }

        protocol::Packet data_packet;
        data_packet.header.type = protocol::PacketType::Data;
        data_packet.payload.assign(buffer.begin(),
                                   buffer.begin() + static_cast<std::ptrdiff_t>(read_count));
        protocol::write_packet(connection_, data_packet);

        sent_size += static_cast<std::uint64_t>(read_count);
        if (progress_callback_) {
            progress_callback_(ProgressEvent{ProgressDirection::Send,
                                             relative_path,
                                             sent_size,
                                             file_size,
                                             file_index,
                                             file_count,
                                             sent_size == file_size});
        }
    }

    if (!input.eof()) {
        throw std::runtime_error("failed while streaming input file: " + source_path.string());
    }

    protocol::Packet end_packet;
    end_packet.header.type = protocol::PacketType::FileEnd;
    protocol::write_packet(connection_, end_packet);
}

void Sender::send_files(const std::vector<filesystem::FileEntry>& entries) const {
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        send_file(entry.source_path, entry.relative_path, index + 1, entries.size());
    }
}

void Sender::send_path(const std::filesystem::path& input_path) const {
    send_files(filesystem::scan_files(input_path));
}

} // namespace beamdrop::transfer
