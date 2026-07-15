#include "beamdrop/filesystem/FileScanner.hpp"
#include "beamdrop/filesystem/FileSystemError.hpp"

#include <algorithm>
#include <stdexcept>

namespace beamdrop::filesystem {
namespace {

std::string to_protocol_path(const std::filesystem::path &path) {
    return path.generic_string();
}

void sort_entries(std::vector<FileEntry> &entries) {
    std::sort(entries.begin(), entries.end(), [](const FileEntry &lhs, const FileEntry &rhs) {
        return lhs.relative_path < rhs.relative_path;
    });
}

} // namespace

std::vector<FileEntry> scan_files(const std::filesystem::path &input_path) {
    if (!std::filesystem::exists(input_path)) {
        throw FileSystemError{ErrorCode::NotFound, input_path, "scan input path does not exist: "};
    }
    if (std::filesystem::is_symlink(input_path)) {
        throw FileSystemError{ErrorCode::NotRegularFileOrDirectory, input_path,
                              "symbolic links are not supported: "};
    }

    std::vector<FileEntry> entries;

    if (std::filesystem::is_regular_file(input_path)) {
        entries.push_back(FileEntry{input_path, to_protocol_path(input_path.filename()),
                                    std::filesystem::file_size(input_path)});
        return entries;
    }

    if (!std::filesystem::is_directory(input_path)) {
        throw FileSystemError{ErrorCode::NotRegularFileOrDirectory, input_path,
                              "scan input path is not a regular file or directory: "};
    }

    const auto normalized_input = input_path.lexically_normal();
    const auto root_name = normalized_input.filename();
    if (root_name.empty()) {
        throw FileSystemError{ErrorCode::NotRegularFileOrDirectory, input_path,
                              "scan input directory must have a name: "};
    }

    for (const auto &item : std::filesystem::recursive_directory_iterator(input_path)) {
        if (item.is_symlink()) {
            throw FileSystemError{ErrorCode::NotRegularFileOrDirectory, item.path(),
                                  "symbolic links are not supported: "};
        }
        if (!item.is_regular_file()) {
            continue;
        }

        const auto relative = std::filesystem::relative(item.path(), input_path);
        entries.push_back(FileEntry{item.path(), to_protocol_path(root_name / relative),
                                    std::filesystem::file_size(item.path())});
    }

    sort_entries(entries);
    return entries;
}

std::vector<DirectoryEntry> scan_directories(const std::filesystem::path &input_path) {
    if (!std::filesystem::exists(input_path)) {
        throw FileSystemError{ErrorCode::NotFound, input_path, "scan input path does not exist: "};
    }
    if (std::filesystem::is_symlink(input_path)) {
        throw FileSystemError{ErrorCode::NotRegularFileOrDirectory, input_path,
                              "symbolic links are not supported: "};
    }
    if (!std::filesystem::is_directory(input_path)) {
        return {};
    }

    const auto root_name = input_path.lexically_normal().filename();
    if (root_name.empty()) {
        throw FileSystemError{ErrorCode::NotRegularFileOrDirectory, input_path,
                              "scan input directory must have a name: "};
    }

    std::vector<DirectoryEntry> entries{{input_path, to_protocol_path(root_name)}};
    for (const auto &item : std::filesystem::recursive_directory_iterator(input_path)) {
        if (item.is_symlink()) {
            throw FileSystemError{ErrorCode::NotRegularFileOrDirectory, item.path(),
                                  "symbolic links are not supported: "};
        }
        if (!item.is_directory()) {
            continue;
        }
        const auto relative = std::filesystem::relative(item.path(), input_path);
        entries.push_back(DirectoryEntry{item.path(), to_protocol_path(root_name / relative)});
    }
    std::sort(entries.begin(), entries.end(), [](const DirectoryEntry &lhs, const DirectoryEntry &rhs) {
        return lhs.relative_path < rhs.relative_path;
    });
    return entries;
}

} // namespace beamdrop::filesystem