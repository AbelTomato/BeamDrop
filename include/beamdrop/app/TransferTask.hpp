#pragma once

#include "beamdrop/transfer/Progress.hpp"

#include <stdexcept>
#include <string>
#include <functional>
#include <cstddef>
#include <cstdint>

namespace beamdrop::app {
struct TransferProgress {
    enum class Direction { Send, Receive };

    Direction direction = Direction::Send;
    std::string relative_path;
    std::uint64_t current_file_bytes = 0;
    std::uint64_t current_file_total_bytes = 0;
    std::size_t file_index = 0;
    std::size_t file_count = 0;
    bool file_complete = false;
};

using ProgressCallback = std::function<void(const TransferProgress &)>;

struct ServiceError {
    enum class Code { InvalidRequest, NetworkFailure, ProtocolError, FileSystemError, Cancelled };
};

class ServiceException : public std::runtime_error {
  public:
    ServiceException(ServiceError::Code code, const std::string &message)
        : std::runtime_error(message), code_(code) {}

    [[nodiscard]] ServiceError::Code code() const noexcept {
        return code_;
    }

  private:
    ServiceError::Code code_;
};

[[nodiscard]] inline TransferProgress
to_app_progress(const beamdrop::transfer::ProgressEvent &event) {
    return TransferProgress {
        event.direction == transfer::ProgressDirection::Send ? TransferProgress::Direction::Send
                                                             : TransferProgress::Direction::Receive,
            event.relative_path, event.current_file_bytes, event.current_file_total_bytes,
            event.file_index, event.file_count,
            event.file_complete,
    };
}
}       // namespace beamdrop::app