#include "beamdrop/app/ReceiveServerService.hpp"
#include "beamdrop/app/ReceiveService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/network/NetworkError.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

namespace {
std::optional<beamdrop::app::ServiceError>
validate_request(const beamdrop::app::ReceiveServerRequest &request) {
    if (request.port == 0 || request.receive_request.save_dir.empty() ||
        request.receive_request.state_file.empty()) {
        return beamdrop::app::ServiceError{
            beamdrop::app::ErrorCode::InvalidRequest,
            "invalid receive server request",
        };
    }
    return std::nullopt;
}
} // namespace

namespace beamdrop::app {

struct ReceiveServerService::State {
    mutable std::mutex mutex;
    ReceiveServerStatus status;
    std::unique_ptr<network::TcpServer> server;
    std::jthread worker;
};

ReceiveServerService::ReceiveServerService() : state_(std::make_shared<State>()) {}

ReceiveServerService::~ReceiveServerService() {
    if (state_) {
        (void)stop();
    }
}

ServiceResult<ReceiveServerStatus>
ReceiveServerService::start(const ReceiveServerRequest &request) {
    if (auto error = validate_request(request)) {
        return ServiceResult<ReceiveServerStatus>::failure(std::move(*error));
    }

    auto state = state_;
    std::lock_guard lock{state->mutex};

    if (state->status.state != ReceiveServerState::Stopped) {
        return ServiceResult<ReceiveServerStatus>::failure(
            {ErrorCode::AlreadyRunning, "receive server is already running"});
    }

    state->status = {.state = ReceiveServerState::Starting};

    try {
        state->server = std::make_unique<network::TcpServer>(request.host, request.port);
    } catch (const network::NetworkError &error) {
        state->status = {
            .state = ReceiveServerState::Failed,
            .last_error = ServiceError{ErrorCode::BindFailed, error.what()},
        };
        return ServiceResult<ReceiveServerStatus>::failure(*state->status.last_error);
    } catch (const std::exception &error) {
        state->status = {
            .state = ReceiveServerState::Failed,
            .last_error = ServiceError{ErrorCode::InternalError, error.what()},
        };
        return ServiceResult<ReceiveServerStatus>::failure(*state->status.last_error);
    }

    const auto receive_request = request.receive_request;
    state->worker = std::jthread{[raw_state = state.get(), receive_request](std::stop_token stop_token) mutable {
        receive_loop(*raw_state, receive_request, stop_token);
    }};
    state->status = {.state = ReceiveServerState::Running, .last_error = {}};
    return ServiceResult<ReceiveServerStatus>::success(state->status);
}

ServiceResult<ReceiveServerStatus> ReceiveServerService::stop() {
    auto state = state_;

    {
        std::lock_guard lock(state->mutex);
        if (state->status.state == ReceiveServerState::Stopped) {
            return ServiceResult<ReceiveServerStatus>::failure(
                {ErrorCode::NotRunning, "receive server is not running"});
        }

        if (state->status.state == ReceiveServerState::Stopping) {
            return ServiceResult<ReceiveServerStatus>::success(state->status);
        }

        if (state->status.state == ReceiveServerState::Failed) {
            state->server.reset();
            state->status.state = ReceiveServerState::Stopped;
            return ServiceResult<ReceiveServerStatus>::success(state->status);
        }

        state->status.state = ReceiveServerState::Stopping;
        state->worker.request_stop();
        return ServiceResult<ReceiveServerStatus>::success(state->status);
    }
}

void ReceiveServerService::receive_loop(State &state, ReceiveRequest request,
                                        std::stop_token stop_token) {
    using namespace std::chrono_literals;
    try {
        while (!stop_token.stop_requested()) {
            auto connection = state.server->accept_one_for(100ms);

            if (!connection) {
                continue;
            }

            ReceiveService receiver;
            auto result = receiver.receive(*connection, request, stop_token);

            if (!result) {
                std::lock_guard lock(state.mutex);
                state.status.last_error = result.error();
            }
        }

        std::lock_guard lock(state.mutex);
        state.server.reset();
        state.status = {.state = ReceiveServerState::Stopped};
    } catch (const network::NetworkError &error) {
        if (stop_token.stop_requested()) {
            std::lock_guard lock(state.mutex);
            state.server.reset();
            state.status = {.state = ReceiveServerState::Stopped};
            return;
        }

        std::lock_guard lock{state.mutex};
        state.server.reset();
        state.status = {.state = ReceiveServerState::Failed,
                         .last_error = ServiceError{ErrorCode::NetworkFailure, error.what()}};
    } catch (const std::exception &error) {
        std::lock_guard lock{state.mutex};
        state.server.reset();
        state.status = {.state = ReceiveServerState::Failed,
                        .last_error = ServiceError{ErrorCode::InternalError, error.what()}};
    }
}

ReceiveServerStatus ReceiveServerService::status() const {
    auto state = state_;
    std::lock_guard lock(state->mutex);
    return state->status;
}
} // namespace beamdrop::app