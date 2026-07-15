#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

namespace beamdrop::filesystem {

enum class ErrorCode {
    NotFound,
    NotRegularFileOrDirectory,
    PermissionDenied,
    MetadataUnavailable,
    DirectoryIterationFailed
};

class FileSystemError : public std::runtime_error {
  public:
    FileSystemError(ErrorCode code, std::filesystem::path path, std::string message)
        : std::runtime_error(std::move(message)), code_(code), path_(std::move(path)) {}

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
} // namespace beamdrop::filesystem