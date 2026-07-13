#pragma once

#include "beamdrop/transfer/Progress.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <variant>

namespace beamdrop::app {
struct TransferProgress {
    enum class Direction { Send, Receive };

    Direction direction = Direction::Send;
    std::string relative_path;
    std::uint64_t current_file_bytes = 0;
    std::uint64_t current_file_total_bytes = 0;
    std::size_t file_index = 0;
    std::size_t file_count = 0;
    beamdrop::transfer::Stage stage = beamdrop::transfer::Stage::TaskStarted;
};

using ProgressCallback = std::function<void(const TransferProgress &)>;

enum class ErrorCode {
    InvalidRequest,
    NetworkFailure,
    ProtocolError,
    FileSystemError,
    Cancelled,
    AlreadyRunning,
    NotRunning,
    BindFailed,
    InternalError
};

struct ServiceError {
    ErrorCode code;
    std::string message;
};

template <typename T> class ServiceResult {
  public:
    static ServiceResult success(T value) {
        return ServiceResult{std::in_place_index<0>, std::move(value)};
    }

    static ServiceResult failure(ServiceError error) {
        return ServiceResult{std::in_place_index<1>, std::move(error)};
    }

    [[nodiscard]] bool has_value() const noexcept {
        return storage_.index() == 0;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] T &value() & {
        return std::get<T>(storage_);
    }

    [[nodiscard]] const T &value() const & {
        return std::get<T>(storage_);
    }

    [[nodiscard]] T &&value() && {
        return std::get<T>(std::move(storage_));
    }

    [[nodiscard]] ServiceError &error() & {
        return std::get<ServiceError>(storage_);
    }

    [[nodiscard]] const ServiceError &error() const & {
        return std::get<ServiceError>(storage_);
    }

  private:
    template <std::size_t Index, typename U>
    explicit ServiceResult(std::in_place_index_t<Index>, U &&value)
        : storage_(std::in_place_index<Index>, std::forward<U>(value)) {}

    std::variant<T, ServiceError> storage_;
};

[[nodiscard]] inline TransferProgress
to_app_progress(const beamdrop::transfer::ProgressEvent &event) {
    return TransferProgress{event.direction == transfer::ProgressDirection::Send
                                ? TransferProgress::Direction::Send
                                : TransferProgress::Direction::Receive,
                            event.relative_path,
                            event.current_file_bytes,
                            event.current_file_total_bytes,
                            event.file_index,
                            event.file_count,
                            event.stage};
}
} // namespace beamdrop::app