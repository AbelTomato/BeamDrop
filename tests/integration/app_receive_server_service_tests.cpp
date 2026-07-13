#include "beamdrop/app/ReceiveServerService.hpp"
#include "beamdrop/app/ReceiveService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include <cassert>
#include <chrono>
#include <functional>
#include <thread>

namespace {
bool wait_until(const std::function<bool()> &condition, std::chrono::milliseconds timeout) {
    using namespace std::chrono_literals;

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!condition()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return condition();
        }
        std::this_thread::sleep_for(10ms);
    }
    return true;
}
} // namespace

int main() {
    beamdrop::app::ReceiveServerService receive_server = beamdrop::app::ReceiveServerService{};

    assert(receive_server.status().state == beamdrop::app::ReceiveServerState::Stopped);

    auto result = receive_server.stop();
    assert(!result);
    assert(result.error().code == beamdrop::app::ErrorCode::NotRunning);

    auto request = beamdrop::app::ReceiveServerRequest{};
    request.host = "127.0.0.1";
    request.receive_request.state_file = "state.json";
    request.receive_request.save_dir = "data/";
    result = receive_server.start(request);
    assert(!result);
    assert(result.error().code == beamdrop::app::ErrorCode::InvalidRequest);
    assert(receive_server.status().state == beamdrop::app::ReceiveServerState::Stopped);

    request.port = 19103;
    request.receive_request.save_dir = "";
    result = receive_server.start(request);
    assert(!result);
    assert(result.error().code == beamdrop::app::ErrorCode::InvalidRequest);
    assert(receive_server.status().state == beamdrop::app::ReceiveServerState::Stopped);

    request.receive_request.save_dir = "data/";
    result = receive_server.start(request);
    assert(result);
    assert(receive_server.status().state == beamdrop::app::ReceiveServerState::Running);

    result = receive_server.start(request);
    assert(!result);
    assert(result.error().code == beamdrop::app::ErrorCode::AlreadyRunning);

    result = receive_server.stop();
    assert(result);
    assert(result.value().state == beamdrop::app::ReceiveServerState::Stopping);
    assert(receive_server.status().state == beamdrop::app::ReceiveServerState::Stopping);

    assert(wait_until(
        [&receive_server] {
            return receive_server.status().state == beamdrop::app::ReceiveServerState::Stopped;
        },
        std::chrono::seconds{2}));

    result = receive_server.stop();
    assert(!result);
    assert(result.error().code == beamdrop::app::ErrorCode::NotRunning);

    result = receive_server.start(request);
    assert(result);
    assert(result.value().state == beamdrop::app::ReceiveServerState::Running);

    beamdrop::app::ReceiveServerService receive_server_2;
    assert(receive_server.status().state == beamdrop::app::ReceiveServerState::Running);
    result = receive_server_2.start(request);
    if (result) {
        result = receive_server_2.stop();
        assert(result);
        assert(wait_until(
            [&receive_server_2] {
                return receive_server_2.status().state == beamdrop::app::ReceiveServerState::Stopped;
            },
            std::chrono::seconds{2}));
    } else {
        assert(result.error().code == beamdrop::app::ErrorCode::BindFailed);
        assert(receive_server_2.status().state == beamdrop::app::ReceiveServerState::Failed);
        result = receive_server_2.stop();
        assert(result);
        assert(result.value().state == beamdrop::app::ReceiveServerState::Stopped);
    }

    result = receive_server.stop();
    assert(result);
    assert(wait_until(
        [&receive_server] {
            return receive_server.status().state == beamdrop::app::ReceiveServerState::Stopped;
        },
        std::chrono::seconds{2}));

    return 0;
}