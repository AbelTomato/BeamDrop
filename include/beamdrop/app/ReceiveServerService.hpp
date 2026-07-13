#pragma once

#include "beamdrop/app/ReceiveService.hpp"
#include "beamdrop/app/TransferTask.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace beamdrop::app {

enum class ReceiveServerState { Stopped, Starting, Running, Stopping, Failed };

struct ReceiveServerRequest {
    std::string host = "0.0.0.0";
    std::uint16_t port = 0;
    ReceiveRequest receive_request;
};

struct ReceiveServerStatus {
    ReceiveServerState state = ReceiveServerState::Stopped;
    std::optional<ServiceError> last_error = std::nullopt;
};

class ReceiveServerService {
  private:
    struct State;
    std::shared_ptr<State> state_;

  public:
    ReceiveServerService();
    ~ReceiveServerService();

    ReceiveServerService(const ReceiveServerService &) = delete;
    ReceiveServerService &operator=(const ReceiveServerService &) = delete;

    [[nodiscard]] ServiceResult<ReceiveServerStatus> start(const ReceiveServerRequest &request);

    [[nodiscard]] ReceiveServerStatus status() const;

    [[nodiscard]] ServiceResult<ReceiveServerStatus> stop();

    static void receive_loop(State &state, ReceiveRequest request, std::stop_token stop_token);
};
} // namespace beamdrop::app