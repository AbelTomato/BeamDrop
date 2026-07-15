#include "beamdrop/app/ReceiveService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/filesystem/FileSystemError.hpp"
#include "beamdrop/network/NetworkError.hpp"
#include "beamdrop/network/TcpConnection.hpp"
#include "beamdrop/protocol/Packet.hpp"
#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/Progress.hpp"
#include "beamdrop/transfer/Receiver.hpp"
#include "beamdrop/transfer/TransferError.hpp"
#include "beamdrop/transfer/TransferManifest.hpp"
#include <exception>
#include <optional>
#include <stop_token>

namespace {
std::optional<beamdrop::app::ServiceError>
validate_receive_request(const beamdrop::app::ReceiveRequest &request) {
    if (request.state_file.empty()) {
        return beamdrop::app::ServiceError{beamdrop::app::ErrorCode::InvalidRequest,
                                           "state file path must not be empty"};
    }

    if (request.save_dir.empty()) {
        return beamdrop::app::ServiceError{beamdrop::app::ErrorCode::InvalidRequest,
                                           "save_dir path must not be empty"};
    }

    return std::nullopt;
}

std::optional<beamdrop::app::ServiceError> check_cancelled(const std::stop_token &stop_token) {
    if (stop_token.stop_requested()) {
        return beamdrop::app::ServiceError{beamdrop::app::ErrorCode::Cancelled,
                                           "receive cancelled"};
    }
    return std::nullopt;
}
} // namespace

namespace beamdrop::app {
ServiceResult<ReceiveResult>
ReceiveService::receive(const beamdrop::network::TcpConnection &connection,
                        const ReceiveRequest &request, const std::stop_token stop_token) const {
    try {
        if (auto error = validate_receive_request(request)) {
            return ServiceResult<ReceiveResult>::failure(std::move(*error));
        }
        if (auto error = check_cancelled(stop_token)) {
            return ServiceResult<ReceiveResult>::failure(std::move(*error));
        }

        const auto hello = beamdrop::protocol::read_packet(connection);
        if (hello.header.type != beamdrop::protocol::PacketType::Hello) {
            return ServiceResult<ReceiveResult>::failure(ServiceError{
                ErrorCode::ProtocolError, "expected HELLO packet with transfer manifest"});
        }

        if (auto error = check_cancelled(stop_token)) {
            return ServiceResult<ReceiveResult>::failure(std::move(*error));
        }

        const auto manifest = beamdrop::transfer::TransferManifestCodec::decode(hello.payload);
        const auto file_count = manifest.file_count;

        auto progress_callback = [&request](const transfer::ProgressEvent &event) {
            if (request.progress_callback) {
                request.progress_callback(to_app_progress(event));
            }
        };

        beamdrop::transfer::Receiver receiver{connection, progress_callback, request.enable_resume,
                                              request.state_file};
        receiver.receive_task(request.save_dir, file_count, manifest.directory_count);

        if (auto error = check_cancelled(stop_token)) {
            return ServiceResult<ReceiveResult>::failure(std::move(*error));
        }

        const auto finish = beamdrop::protocol::read_packet(connection);
        if (finish.header.type != beamdrop::protocol::PacketType::Finish) {
            return ServiceResult<ReceiveResult>::failure(
                ServiceError{ErrorCode::ProtocolError, "expected FINISH packet"});
        }

        return ServiceResult<ReceiveResult>::success(ReceiveResult{file_count});
    } catch (const beamdrop::filesystem::FileSystemError &error) {
        return ServiceResult<ReceiveResult>::failure(
            ServiceError{ErrorCode::FileSystemError, error.what()});
    } catch (const beamdrop::network::NetworkError &error) {
        return ServiceResult<ReceiveResult>::failure(
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

        default:
            service_code = ErrorCode::ProtocolError;
            break;
        }

        return ServiceResult<ReceiveResult>::failure({service_code, error.what()});
    } catch (const std::exception &error) {
        return ServiceResult<ReceiveResult>::failure(
            ServiceError{ErrorCode::InternalError, error.what()});
    }
}
} // namespace beamdrop::app