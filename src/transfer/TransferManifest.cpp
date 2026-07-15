#include "beamdrop/transfer/TransferManifest.hpp"
#include "beamdrop/transfer/TransferError.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>

namespace beamdrop::transfer {
namespace {

std::uint64_t parse_u64_field(const std::string &text, const std::string &field_name) {
    if (text.empty()) {
        throw TransferError{ErrorCode::InvalidPayload, "invalid transfer manifest " + field_name};
    }

    try {
        std::size_t parsed_chars = 0;
        const auto value = std::stoull(text, &parsed_chars);
        if (parsed_chars != text.size()) {
            throw TransferError{ErrorCode::InvalidPayload,
                                "invalid transfer manifest " + field_name};
        }
        return static_cast<std::uint64_t>(value);
    } catch (const std::exception &) {
        throw TransferError{ErrorCode::InvalidPayload, "invalid transfer manifest " + field_name};
    }
}

} // namespace

std::vector<std::uint8_t> TransferManifestCodec::encode(const TransferManifest &manifest) {
    std::ostringstream stream;
    stream << "beamdrop-manifest-v2" << '\n'
           << "file_count=" << manifest.file_count << '\n'
           << "total_bytes=" << manifest.total_bytes << '\n'
           << "directory_count=" << manifest.directory_count << '\n';
    const auto text = stream.str();
    return {text.begin(), text.end()};
}

TransferManifest TransferManifestCodec::decode(const std::vector<std::uint8_t> &payload) {
    const std::string text{payload.begin(), payload.end()};
    std::istringstream stream{text};

    std::string version;
    std::string file_count_line;
    std::string total_bytes_line;
    std::string directory_count_line;
    std::getline(stream, version);
    std::getline(stream, file_count_line);
    std::getline(stream, total_bytes_line);
    std::getline(stream, directory_count_line);

    if (version != "beamdrop-manifest-v2") {
        throw TransferError{ErrorCode::InvalidPayload, "invalid transfer manifest version"};
    }

    constexpr std::string_view file_count_prefix{"file_count="};
    constexpr std::string_view total_bytes_prefix{"total_bytes="};
    constexpr std::string_view directory_count_prefix{"directory_count="};
    if (!file_count_line.starts_with(file_count_prefix) ||
        !total_bytes_line.starts_with(total_bytes_prefix) ||
        !directory_count_line.starts_with(directory_count_prefix)) {
        throw TransferError{ErrorCode::InvalidPayload, "invalid transfer manifest payload"};
    }

    TransferManifest manifest;
    manifest.file_count =
        parse_u64_field(file_count_line.substr(file_count_prefix.size()), "file_count");
    manifest.total_bytes =
        parse_u64_field(total_bytes_line.substr(total_bytes_prefix.size()), "total_bytes");
    manifest.directory_count = parse_u64_field(
        directory_count_line.substr(directory_count_prefix.size()), "directory_count");
    return manifest;
}

} // namespace beamdrop::transfer