#pragma once

#include <filesystem>
#include <stdexcept>
namespace beamdrop::transfer {
enum class ErrorCode {
    InvalidConfiguration,
    InvalidPayload,
    UnexpectedPacket,
    UnsafePath,
    InvalidResumeState,
    InvalidResumeOffset,
    FileOpenFailed,
    FileReadFailed,
    FileWriteFailed,
    FileCloseFailed,
    SizeMismatch,
    ChecksumMismatch,
};

class TransferError : public std::runtime_error {
  public:
    TransferError(ErrorCode code, std::string message, std::filesystem::path path = {})
        : std::runtime_error(std::move(message)), code_(code), path_(path) {}

    [[nodiscard]] ErrorCode code() const noexcept {
        return code_;
    }

    [[nodiscard]] const std::filesystem::path &path() const noexcept {
        return path_;
    }

  private:
    ErrorCode code_;
    std::filesystem::path path_;
};
} // namespace beamdrop::transfer