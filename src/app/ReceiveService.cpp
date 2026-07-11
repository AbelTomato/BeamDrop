#include "beamdrop/app/ReceiveService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/network/TcpConnection.hpp"
#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/Progress.hpp"
#include "beamdrop/transfer/Receiver.hpp"
#include "beamdrop/transfer/TransferManifest.hpp"
#include <stdexcept>
#include <stop_token>

namespace {
void validate_receive_request(const beamdrop::app::ReceiveRequest &request) {
    if (request.state_file.empty()) {
        throw beamdrop::app::ServiceException {
            beamdrop::app::ServiceError::Code::InvalidRequest,
                "state file path must not be empty"
        };
    }

    if (request.save_dir.empty()) {
        throw beamdrop::app::ServiceException {
            beamdrop::app::ServiceError::Code::InvalidRequest,
                "save_dir path must not be empty"
        };
    }
}

void throw_if_cancelled(const std::stop_token &stop_token) {
    if (stop_token.stop_requested()) {
        throw beamdrop::app::ServiceException {
            beamdrop::app::ServiceError::Code::Cancelled,
                "receive cancelled"
        };
    }
}
}   // namespace

namespace beamdrop::app {
ReceiveResult ReceiveService::receive(const beamdrop::network::TcpConnection& connection, const ReceiveRequest &request) const {
    validate_receive_request(request);
    throw_if_cancelled(request.stop_token);

    const auto hello = beamdrop::protocol::read_packet(connection);
    if (hello.header.type != beamdrop::protocol::PacketType::Hello) {
        throw ServiceException{ServiceError::Code::ProtocolError, "expected HELLO packet with transfer manifest"};
    }

    throw_if_cancelled(request.stop_token);

    const auto manifest = beamdrop::transfer::TransferManifestCodec::decode(hello.payload);
    const auto file_count = manifest.file_count;

    auto progress_callback = [&request](const transfer::ProgressEvent &event) {
        if (request.progress_callback) {
            request.progress_callback(to_app_progress(event));
        }
    };

    beamdrop::transfer::Receiver receiver{connection, progress_callback, request.enable_resume,
                                          request.state_file};
    receiver.receive_files(request.save_dir, file_count);

    throw_if_cancelled(request.stop_token);

    const auto finish = beamdrop::protocol::read_packet(connection);
    if (finish.header.type != beamdrop::protocol::PacketType::Finish) {
        throw ServiceException{ServiceError::Code::ProtocolError, "expected FINISH packet"};
    }

    return ReceiveResult{file_count};
}
}   // namespace beamdrop::app