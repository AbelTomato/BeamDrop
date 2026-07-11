#include "beamdrop/app/SendService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/filesystem/FileScanner.hpp"
#include "beamdrop/network/TcpClient.hpp"
#include "beamdrop/protocol/Packet.hpp"
#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/Progress.hpp"
#include "beamdrop/transfer/Sender.hpp"
#include "beamdrop/transfer/TransferManifest.hpp"
#include <cstdint>
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

void validate_send_request(const beamdrop::app::SendRequest &request) {
    if (request.paths.size() == 0) {
        throw beamdrop::app::ServiceException {
            beamdrop::app::ServiceError::Code::InvalidRequest,
                "paths list must not be empty"
        };
    }

    for (const auto &path : request.paths) {
        if (path.empty()) {
            throw beamdrop::app::ServiceException {
                beamdrop::app::ServiceError::Code::InvalidRequest,
                    "file path must not be empty"
            };
        }
    }

    if (request.host.empty()) {
        throw beamdrop::app::ServiceException {
            beamdrop::app::ServiceError::Code::InvalidRequest,
                "host must not be empty"
        };
    }

    if (request.port == 0) {
        throw beamdrop::app::ServiceException {
            beamdrop::app::ServiceError::Code::InvalidRequest,
                "client port must not be 0"
        };
    }

    if (request.chunk_size == 0) {
        throw beamdrop::app::ServiceException {
            beamdrop::app::ServiceError::Code::InvalidRequest,
                "chunk_size must not be 0"
        };
    }
}

void throw_if_cancelled(const std::stop_token &stop_token) {
    if (stop_token.stop_requested()) {
        throw beamdrop::app::ServiceException {
            beamdrop::app::ServiceError::Code::Cancelled,
                "send cancelled"
        };
    }
}
}   // namespace

namespace beamdrop::app {
SendResult SendService::send(const SendRequest &request) const {
    validate_send_request(request);
    throw_if_cancelled(request.stop_token);

    std::vector<beamdrop::filesystem::FileEntry> entries;
    for (const auto &path : request.paths) {
        auto scanned = beamdrop::filesystem::scan_files(path);
        entries.insert(entries.end(), scanned.begin(), scanned.end());
    }

    throw_if_cancelled(request.stop_token);

    const auto total_bytes = total_entry_bytes(entries);

    beamdrop::network::TcpClient client;
    auto connection = client.connect(request.host, request.port);

    throw_if_cancelled(request.stop_token);

    beamdrop::protocol::Packet hello;
    hello.header.type = beamdrop::protocol::PacketType::Hello;
    const beamdrop::transfer::TransferManifest manifest{static_cast<uint64_t>(entries.size()), total_bytes};
    hello.payload = beamdrop::transfer::TransferManifestCodec::encode(manifest);
    beamdrop::protocol::write_packet(connection, hello);

    auto progress_callback = [&request](const beamdrop::transfer::ProgressEvent &event) {
        if (request.progress_callback) {
            request.progress_callback(to_app_progress(event));
        }
    };

    beamdrop::transfer::Sender sender{connection, progress_callback, request.chunk_size};
    sender.send_files(entries);

    throw_if_cancelled(request.stop_token);

    beamdrop::protocol::Packet finish;
    finish.header.type = beamdrop::protocol::PacketType::Finish;
    beamdrop::protocol::write_packet(connection, finish);

    return SendResult{entries.size(), total_bytes};
}
}   // namespace beamdrop::app