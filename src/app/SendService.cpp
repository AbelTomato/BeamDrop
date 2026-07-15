#include "beamdrop/app/SendService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/filesystem/FileScanner.hpp"
#include "beamdrop/filesystem/FileSystemError.hpp"
#include "beamdrop/network/NetworkError.hpp"
#include "beamdrop/network/TcpClient.hpp"
#include "beamdrop/protocol/Packet.hpp"
#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/Progress.hpp"
#include "beamdrop/transfer/Sender.hpp"
#include "beamdrop/transfer/TransferError.hpp"
#include "beamdrop/transfer/TransferManifest.hpp"
#include <cstdint>
#include <exception>
#include <optional>
#include <set>
#include <stop_token>
#include <vector>

namespace {
std::uint64_t total_entry_bytes(const std::vector<beamdrop::filesystem::FileEntry> &entries) {
    std::uint64_t total = 0;
    for (const auto &entry : entries) {
        total += entry.size;
    }
    return total;
}

std::optional<beamdrop::app::ServiceError>
validate_unique_targets(const std::vector<beamdrop::filesystem::FileEntry> &files,
                        const std::vector<beamdrop::filesystem::DirectoryEntry> &directories) {
    std::set<std::string> paths;
    for (const auto &directory : directories) {
        if (!paths.insert(directory.relative_path).second) {
            return beamdrop::app::ServiceError{beamdrop::app::ErrorCode::InvalidRequest,
                                               "duplicate receive target: " + directory.relative_path};
        }
    }
    for (const auto &file : files) {
        if (!paths.insert(file.relative_path).second) {
            return beamdrop::app::ServiceError{beamdrop::app::ErrorCode::InvalidRequest,
                                               "duplicate receive target: " + file.relative_path};
        }
    }
    return std::nullopt;
}

std::optional<beamdrop::app::ServiceError>
validate_send_request(const beamdrop::app::SendRequest &request) {
    if (request.paths.size() == 0) {
        return beamdrop::app::ServiceError{beamdrop::app::ErrorCode::InvalidRequest,
                                           "paths list must not be empty"};
    }

    for (const auto &path : request.paths) {
        if (path.empty()) {
            return beamdrop::app::ServiceError{beamdrop::app::ErrorCode::InvalidRequest,
                                               "file path must not be empty"};
        }
    }

    if (request.host.empty()) {
        return beamdrop::app::ServiceError{beamdrop::app::ErrorCode::InvalidRequest,
                                           "host must not be empty"};
    }

    if (request.port == 0) {
        return beamdrop::app::ServiceError{beamdrop::app::ErrorCode::InvalidRequest,
                                           "client port must not be 0"};
    }

    if (request.chunk_size == 0) {
        return beamdrop::app::ServiceError{beamdrop::app::ErrorCode::InvalidRequest,
                                           "chunk_size must not be 0"};
    }

    return std::nullopt;
}

std::optional<beamdrop::app::ServiceError> check_cancelled(const std::stop_token &stop_token) {
    if (stop_token.stop_requested()) {
        return beamdrop::app::ServiceError{beamdrop::app::ErrorCode::Cancelled, "send cancelled"};
    }
    return std::nullopt;
}
} // namespace

namespace beamdrop::app {
ServiceResult<SendResult> SendService::send(const SendRequest &request) const {
    try {
        if (auto error = validate_send_request(request)) {
            return ServiceResult<SendResult>::failure(std::move(*error));
        }
        if (auto error = check_cancelled(request.stop_token)) {
            return ServiceResult<SendResult>::failure(std::move(*error));
        }

        std::vector<beamdrop::filesystem::FileEntry> entries;
        std::vector<beamdrop::filesystem::DirectoryEntry> directories;
        for (const auto &path : request.paths) {
            auto scanned = beamdrop::filesystem::scan_files(path);
            entries.insert(entries.end(), scanned.begin(), scanned.end());
            auto scanned_directories = beamdrop::filesystem::scan_directories(path);
            directories.insert(directories.end(), scanned_directories.begin(), scanned_directories.end());
        }

        if (auto error = validate_unique_targets(entries, directories)) {
            return ServiceResult<SendResult>::failure(std::move(*error));
        }

        if (auto error = check_cancelled(request.stop_token)) {
            return ServiceResult<SendResult>::failure(std::move(*error));
        }

        const auto total_bytes = total_entry_bytes(entries);

        beamdrop::network::TcpClient client;
        auto connection = client.connect(request.host, request.port);

        if (auto error = check_cancelled(request.stop_token)) {
            return ServiceResult<SendResult>::failure(std::move(*error));
        }

        beamdrop::protocol::Packet hello;
        hello.header.type = beamdrop::protocol::PacketType::Hello;
        const beamdrop::transfer::TransferManifest manifest{static_cast<uint64_t>(entries.size()),
                                                             total_bytes,
                                                             static_cast<uint64_t>(directories.size())};
        hello.payload = beamdrop::transfer::TransferManifestCodec::encode(manifest);
        beamdrop::protocol::write_packet(connection, hello);

        auto progress_callback = [&request](const beamdrop::transfer::ProgressEvent &event) {
            if (request.progress_callback) {
                request.progress_callback(to_app_progress(event));
            }
        };

        beamdrop::transfer::Sender sender{connection, progress_callback, request.chunk_size,
                                          request.stop_token};
        sender.send_task(entries, directories);

        if (auto error = check_cancelled(request.stop_token)) {
            return ServiceResult<SendResult>::failure(std::move(*error));
        }

        beamdrop::protocol::Packet finish;
        finish.header.type = beamdrop::protocol::PacketType::Finish;
        beamdrop::protocol::write_packet(connection, finish);

        return ServiceResult<SendResult>::success(SendResult{entries.size(), total_bytes});
    } catch (const beamdrop::filesystem::FileSystemError &error) {
        return ServiceResult<SendResult>::failure(
            ServiceError{ErrorCode::FileSystemError, error.what()});
    } catch (const beamdrop::network::NetworkError &error) {
        return ServiceResult<SendResult>::failure(
            ServiceError{ErrorCode::NetworkFailure, error.what()});
    } catch (const transfer::TransferError &error) {
        ErrorCode service_code;

        switch (error.code()) {
        case beamdrop::transfer::ErrorCode::FileOpenFailed:
        case beamdrop::transfer::ErrorCode::FileCloseFailed:
        case beamdrop::transfer::ErrorCode::FileReadFailed:
        case beamdrop::transfer::ErrorCode::FileWriteFailed:
            service_code = ErrorCode::FileSystemError;
            break;

        case beamdrop::transfer::ErrorCode::InvalidConfiguration:
            service_code = ErrorCode::InvalidRequest;
            break;

        case beamdrop::transfer::ErrorCode::Cancelled:
            service_code = ErrorCode::Cancelled;
            break;

        default:
            service_code = ErrorCode::ProtocolError;
            break;
        }

        return ServiceResult<SendResult>::failure({service_code, error.what()});
    } catch (const std::exception &error) {
        return ServiceResult<SendResult>::failure(
            ServiceError{ErrorCode::InternalError, error.what()});
    }
}
} // namespace beamdrop::app