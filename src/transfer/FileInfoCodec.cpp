#include "beamdrop/transfer/FileInfoCodec.hpp"

#include <cstddef>
#include <sstream>
#include <stdexcept>

namespace beamdrop::transfer {

std::vector<std::uint8_t> FileInfoCodec::encode(const FileInfo& info) {
    std::ostringstream stream;
    stream << info.relative_path << '\n' << info.size << '\n' << info.sha256 << '\n';
    const auto text = stream.str();
    return {text.begin(), text.end()};
}

FileInfo FileInfoCodec::decode(const std::vector<std::uint8_t>& payload) {
    const std::string text{payload.begin(), payload.end()};
    std::istringstream stream{text};

    FileInfo info;
    std::string size_text;
    std::getline(stream, info.relative_path);
    std::getline(stream, size_text);
    std::getline(stream, info.sha256);

    if (info.relative_path.empty() || size_text.empty() || info.sha256.empty()) {
        throw std::runtime_error("invalid FILE_INFO payload");
    }

    try {
        std::size_t parsed_chars = 0;
        info.size = static_cast<std::uint64_t>(std::stoull(size_text, &parsed_chars));
        if (parsed_chars != size_text.size()) {
            throw std::runtime_error("invalid FILE_INFO size");
        }
    } catch (const std::exception&) {
        throw std::runtime_error("invalid FILE_INFO size");
    }

    return info;
}

} // namespace beamdrop::transfer
