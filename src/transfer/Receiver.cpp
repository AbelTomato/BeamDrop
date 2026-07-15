#include "beamdrop/transfer/Receiver.hpp"

#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/FileInfoCodec.hpp"
#include "beamdrop/transfer/DirectoryInfoCodec.hpp"
#include "beamdrop/transfer/Progress.hpp"
#include "beamdrop/transfer/ResumeAckCodec.hpp"
#include "beamdrop/transfer/ResumeManager.hpp"
#include "beamdrop/transfer/TransferError.hpp"
#include "beamdrop/utils/Sha256.hpp"

#include <fstream>
#include <string>
#include <utility>

namespace beamdrop::transfer {
namespace {

std::filesystem::path safe_join(const std::filesystem::path &save_dir,
                                const std::string &relative_path) {
    const std::filesystem::path path{relative_path};
    if (path.is_absolute()) {
        throw TransferError{ErrorCode::UnsafePath, "absolute receive path is not allowed"};
    }

    for (const auto &part : path) {
        if (part == "..") {
            throw TransferError{ErrorCode::UnsafePath, "path traversal is not allowed"};
        }
    }

    return save_dir / path;
}

std::filesystem::path safe_directory_path(const std::filesystem::path &save_dir,
                                          const std::string &relative_path) {
    const auto path = safe_join(save_dir, relative_path);
    if (path.filename().empty()) {
        throw TransferError{ErrorCode::UnsafePath, "directory path must name a directory"};
    }
    return path;
}

std::uint64_t existing_file_size(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) {
        return 0;
    }
    if (!std::filesystem::is_regular_file(path)) {
        return 0;
    }
    return static_cast<std::uint64_t>(std::filesystem::file_size(path));
}

std::uint64_t trusted_resume_offset(const ResumeManager &resume_manager, const FileInfo &file_info,
                                    const std::filesystem::path &output_path) {
    const auto stored_offset =
        resume_manager.find_offset(file_info.relative_path, file_info.size, file_info.sha256)
            .value_or(0);
    const auto on_disk_size = existing_file_size(output_path);
    return std::min({stored_offset, on_disk_size, file_info.size});
}

} // namespace

Receiver::Receiver(const network::TcpConnection &connection, ProgressCallback progress_callback,
                   bool enable_resume, std::filesystem::path state_file)
    : connection_(connection), progress_callback_(std::move(progress_callback)),
      enable_resume_(enable_resume), state_file_(std::move(state_file)) {}

void Receiver::receive_file(const std::filesystem::path &save_dir) const {
    receive_task(save_dir, 1);
}

void Receiver::receive_one_file(const std::filesystem::path &save_dir, std::size_t file_index,
                                std::size_t file_count) const {
    const auto info_packet = protocol::read_packet(connection_);
    if (info_packet.header.type != protocol::PacketType::FileInfo) {
        throw TransferError{ErrorCode::UnexpectedPacket, "expected FILE_INFO packet"};
    }

    const auto file_info = FileInfoCodec::decode(info_packet.payload);
    const auto output_path = safe_join(save_dir, file_info.relative_path);
    if (std::filesystem::exists(output_path) && !enable_resume_) {
        throw TransferError{ErrorCode::FileWriteFailed,
                            "receive target already exists: " + output_path.string()};
    }
    const auto parent = output_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    const ResumeManager resume_manager{state_file_};
    const auto resume_offset =
        enable_resume_ ? trusted_resume_offset(resume_manager, file_info, output_path) : 0;

    protocol::Packet ack_packet;
    ack_packet.header.type = protocol::PacketType::ResumeAck;
    ack_packet.payload = ResumeAckCodec::encode(ResumeAck{resume_offset});
    protocol::write_packet(connection_, ack_packet);

    if (resume_offset > 0) {
        std::filesystem::resize_file(output_path, resume_offset);
    }

    const auto open_mode = resume_offset > 0 ? (std::ios::binary | std::ios::app)
                                             : (std::ios::binary | std::ios::trunc);
    std::ofstream output(output_path, open_mode);
    if (!output) {
        throw TransferError{ErrorCode::FileOpenFailed,
                            "failed to open output file: " + output_path.string()};
    }

    std::uint64_t received_size = resume_offset;

    while (true) {
        const auto packet = protocol::read_packet(connection_);
        if (packet.header.type == protocol::PacketType::FileEnd) {
            break;
        }
        if (packet.header.type != protocol::PacketType::Data) {
            throw TransferError{ErrorCode::UnexpectedPacket, "expected DATA or FILE_END packet"};
        }

        received_size += packet.payload.size();
        if (received_size > file_info.size) {
            throw TransferError{ErrorCode::SizeMismatch,
                                "received data size exceeds FILE_INFO size"};
        }

        if (!packet.payload.empty()) {
            output.write(reinterpret_cast<const char *>(packet.payload.data()),
                         static_cast<std::streamsize>(packet.payload.size()));
            if (!output) {
                throw TransferError{ErrorCode::FileWriteFailed,
                                    "failed to write output file: " + output_path.string()};
            }
        }

        if (enable_resume_) {
            resume_manager.update_offset(file_info.relative_path, file_info.size, file_info.sha256,
                                         received_size);
        }

        if (progress_callback_) {
            progress_callback_(ProgressEvent{ProgressDirection::Receive, file_info.relative_path,
                                             received_size, file_info.size, file_index, file_count,
                                             Stage::Transferring});
        }
    }

    output.close();
    if (!output) {
        throw TransferError{ErrorCode::FileCloseFailed,
                            "failed to close output file: " + output_path.string()};
    }

    if (received_size != file_info.size) {
        throw TransferError{ErrorCode::SizeMismatch, "received data size mismatch"};
    }
    if (utils::sha256_file(output_path, 1024 * 1024) != file_info.sha256) {
        throw TransferError{ErrorCode::ChecksumMismatch, "received data sha256 mismatch"};
    }

    if (enable_resume_) {
        resume_manager.clear(file_info.relative_path, file_info.size, file_info.sha256);
    }

    if (progress_callback_) {
        progress_callback_(ProgressEvent{ProgressDirection::Receive, file_info.relative_path,
                                         file_info.size, file_info.size, file_index, file_count,
                                         Stage::FileCompleted});
    }
}

void Receiver::receive_one_directory(const std::filesystem::path &save_dir) const {
    const auto info_packet = protocol::read_packet(connection_);
    if (info_packet.header.type != protocol::PacketType::DirectoryInfo) {
        throw TransferError{ErrorCode::UnexpectedPacket, "expected DIRECTORY_INFO packet"};
    }

    const auto output_path = safe_directory_path(save_dir, DirectoryInfoCodec::decode(info_packet.payload));
    if (std::filesystem::exists(output_path)) {
        throw TransferError{ErrorCode::FileWriteFailed,
                            "receive target already exists: " + output_path.string()};
    }
    std::filesystem::create_directories(output_path);
}

void Receiver::receive_task(const std::filesystem::path &save_dir, std::size_t file_count,
                            std::size_t directory_count) const {
    if (progress_callback_) {
        progress_callback_(ProgressEvent{ProgressDirection::Receive, "", 0, 0, 0, file_count,
                                         Stage::TaskStarted});
    }
    for (std::size_t index = 0; index < directory_count; ++index) {
        receive_one_directory(save_dir);
    }
    for (std::size_t index = 0; index < file_count; ++index) {
        receive_one_file(save_dir, index + 1, file_count);
    }
    if (progress_callback_) {
        progress_callback_(ProgressEvent{ProgressDirection::Receive, "", 0, 0, file_count,
                                         file_count, Stage::TaskCompleted});
    }
}

} // namespace beamdrop::transfer
