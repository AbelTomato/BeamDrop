#include "beamdrop/transfer/FileInfoCodec.hpp"

#include <cstddef>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace beamdrop::transfer {
namespace {

std::string escape_json_string(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const auto ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

std::string unescape_json_string(const std::string& value) {
    std::string unescaped;
    unescaped.reserve(value.size());
    bool escaping = false;
    for (const auto ch : value) {
        if (escaping) {
            if (ch != '\\' && ch != '"') {
                throw std::runtime_error("invalid FILE_INFO payload");
            }
            unescaped += ch;
            escaping = false;
            continue;
        }

        if (ch == '\\') {
            escaping = true;
            continue;
        }

        unescaped += ch;
    }

    if (escaping) {
        throw std::runtime_error("invalid FILE_INFO payload");
    }
    return unescaped;
}

std::string require_string_field(const std::string& json, const std::string& name) {
    const std::regex pattern{"\\\"" + name + "\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"\\\\])*)\\\""};
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        throw std::runtime_error("invalid FILE_INFO payload");
    }
    return unescape_json_string(match[1].str());
}

std::uint64_t require_u64_field(const std::string& json, const std::string& name) {
    const std::regex pattern{"\\\"" + name + "\\\"\\s*:\\s*([^,}\\s]+)"};
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        throw std::runtime_error("invalid FILE_INFO payload");
    }

    try {
        std::size_t parsed_chars = 0;
        const auto value = std::stoull(match[1].str(), &parsed_chars);
        if (parsed_chars != match[1].str().size()) {
            throw std::runtime_error("invalid FILE_INFO size");
        }
        return static_cast<std::uint64_t>(value);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid FILE_INFO size");
    }
}

} // namespace

std::vector<std::uint8_t> FileInfoCodec::encode(const FileInfo& info) {
    std::ostringstream stream;
    stream << "{\n"
           << "  \"relative_path\": \"" << escape_json_string(info.relative_path) << "\",\n"
           << "  \"size\": " << info.size << ",\n"
           << "  \"sha256\": \"" << escape_json_string(info.sha256) << "\"\n"
           << "}\n";
    const auto text = stream.str();
    return {text.begin(), text.end()};
}

FileInfo FileInfoCodec::decode(const std::vector<std::uint8_t>& payload) {
    FileInfo info;
    const std::string text{payload.begin(), payload.end()};

    const auto first_non_space = text.find_first_not_of(" \t\r\n");
    if (first_non_space == std::string::npos || text[first_non_space] != '{') {
        throw std::runtime_error("invalid FILE_INFO payload");
    }

    info.relative_path = require_string_field(text, "relative_path");
    info.size = require_u64_field(text, "size");
    info.sha256 = require_string_field(text, "sha256");

    if (info.relative_path.empty() || info.sha256.empty()) {
        throw std::runtime_error("invalid FILE_INFO payload");
    }

    return info;
}

} // namespace beamdrop::transfer
