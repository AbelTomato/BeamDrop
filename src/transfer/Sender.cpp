#include "beamdrop/transfer/Sender.hpp"

#include "beamdrop/protocol/Packet.hpp"
#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/FileInfoCodec.hpp"
#include "beamdrop/transfer/DirectoryInfoCodec.hpp"
#include "beamdrop/transfer/Progress.hpp"
#include "beamdrop/transfer/ResumeAckCodec.hpp"
#include "beamdrop/transfer/TransferError.hpp"
#include "beamdrop/utils/Sha256.hpp"

#include <fstream>
#include <utility>

namespace beamdrop::transfer {

namespace {

filesystem::FileEntry make_single_file_entry(const std::filesystem::path &source_path,
                                             const std::string &relative_path) {
    return filesystem::FileEntry{source_path, relative_path, std::filesystem::file_size(source_path)};
}

} // namespace

Sender::Sender(const network::TcpConnection &connection, ProgressCallback progress_callback,
               std::size_t chunk_size, std::stop_token stop_token)
    : connection_(connection), progress_callback_(std::move(progress_callback)),
      chunk_size_(chunk_size), stop_token_(stop_token) {
    if (chunk_size_ == 0) {
        throw TransferError{ErrorCode::InvalidConfiguration,
                            "sender chunk_size must be greater than zero"};
    }
}

void Sender::send_file(const std::filesystem::path &source_path,
                       const std::string &relative_path) const {
    send_task({make_single_file_entry(source_path, relative_path)});
}

void Sender::send_one_file(const std::filesystem::path &source_path, const std::string &relative_path,
                           std::size_t file_index, std::size_t file_count) const {
    if (stop_token_.stop_requested()) {
        throw TransferError{ErrorCode::Cancelled, "send cancelled"};
    }
    const auto file_size = std::filesystem::file_size(source_path);
    const auto file_hash = utils::sha256_file(source_path, chunk_size_, stop_token_);

    protocol::Packet info_packet;
    info_packet.header.type = protocol::PacketType::FileInfo;
    info_packet.payload = FileInfoCodec::encode(FileInfo{relative_path, file_size, file_hash});
    protocol::write_packet(connection_, info_packet);

    const auto ack_packet = protocol::read_packet(connection_);
    if (ack_packet.header.type != protocol::PacketType::ResumeAck) {
        throw TransferError{ErrorCode::UnexpectedPacket, "expected RESUME_ACK packet"};
    }

    const auto resume_ack = ResumeAckCodec::decode(ack_packet.payload);
    if (resume_ack.offset > file_size) {
        throw TransferError{ErrorCode::InvalidResumeOffset,
                            "resume offset exceeds local file size"};
    }

    std::ifstream input(source_path, std::ios::binary);
    if (!input) {
        throw TransferError{ErrorCode::FileOpenFailed,
                            "failed to open input file for streaming: " + source_path.string()};
    }
    input.seekg(static_cast<std::streamoff>(resume_ack.offset), std::ios::beg);
    if (!input) {
        throw TransferError{ErrorCode::FileReadFailed,
                            "failed to seek input file for resume: " + source_path.string()};
    }

    std::vector<std::uint8_t> buffer(chunk_size_);
    std::uint64_t sent_size = resume_ack.offset;
    while (input) {
        if (stop_token_.stop_requested()) {
            throw TransferError{ErrorCode::Cancelled, "send cancelled"};
        }
        input.read(reinterpret_cast<char *>(buffer.data()),
                   static_cast<std::streamsize>(buffer.size()));
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
            progress_callback_(ProgressEvent{ProgressDirection::Send, relative_path, sent_size,
                                             file_size, file_index, file_count,
                                             Stage::Transferring});
        }
    }

    if (!input.eof()) {
        throw TransferError{ErrorCode::FileReadFailed,
                            "failed while streaming input file: " + source_path.string()};
    }

    protocol::Packet end_packet;
    end_packet.header.type = protocol::PacketType::FileEnd;
    protocol::write_packet(connection_, end_packet);

    if (progress_callback_) {
        progress_callback_(ProgressEvent{ProgressDirection::Send, relative_path, file_size,
                                         file_size, file_index, file_count, Stage::FileCompleted});
    }
}

void Sender::send_one_directory(const std::string &relative_path) const {
    protocol::Packet info_packet;
    info_packet.header.type = protocol::PacketType::DirectoryInfo;
    info_packet.payload = DirectoryInfoCodec::encode(relative_path);
    protocol::write_packet(connection_, info_packet);
}

void Sender::send_task(const std::vector<filesystem::FileEntry> &entries,
                       const std::vector<filesystem::DirectoryEntry> &directories) const {
    if (progress_callback_) {
        progress_callback_(ProgressEvent{ProgressDirection::Send, "", 0, 0, 0, entries.size(),
                                         Stage::TaskStarted});
    }
    for (const auto &directory : directories) {
        if (stop_token_.stop_requested()) {
            throw TransferError{ErrorCode::Cancelled, "send cancelled"};
        }
        send_one_directory(directory.relative_path);
    }
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (stop_token_.stop_requested()) {
            throw TransferError{ErrorCode::Cancelled, "send cancelled"};
        }
        const auto &entry = entries[index];
        send_one_file(entry.source_path, entry.relative_path, index + 1, entries.size());
    }
    if (progress_callback_) {
        progress_callback_(ProgressEvent{ProgressDirection::Send, "", 0, 0, entries.size(),
                                         entries.size(), Stage::TaskCompleted});
    }
}

void Sender::send_path(const std::filesystem::path &input_path) const {
    send_task(filesystem::scan_files(input_path), filesystem::scan_directories(input_path));
}

} // namespace beamdrop::transfer
