#include "beamdrop/transfer/ResumeManager.hpp"

#include <cstddef>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

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
                throw std::runtime_error("invalid resume state file");
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
        throw std::runtime_error("invalid resume state file");
    }
    return unescaped;
}

std::string require_string_field(const std::string& json, const std::string& name) {
    const std::regex pattern{"\\\"" + name + "\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"\\\\])*)\\\""};
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        throw std::runtime_error("invalid resume state file");
    }
    return unescape_json_string(match[1].str());
}

std::uint64_t require_u64_field(const std::string& json, const std::string& name) {
    const std::regex pattern{"\\\"" + name + "\\\"\\s*:\\s*([0-9]+)"};
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        throw std::runtime_error("invalid resume state file");
    }

    try {
        std::size_t parsed_chars = 0;
        const auto value = std::stoull(match[1].str(), &parsed_chars);
        if (parsed_chars != match[1].str().size()) {
            throw std::runtime_error("invalid resume state number");
        }
        return static_cast<std::uint64_t>(value);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid resume state number");
    }
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open resume state file: " + path.string());
    }

    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

bool same_identity(const ResumeRecord& record,
                   const std::string& relative_path,
                   std::uint64_t size,
                   const std::string& sha256) {
    return record.relative_path == relative_path && record.size == size && record.sha256 == sha256;
}

} // namespace

ResumeManager::ResumeManager(std::filesystem::path state_file)
    : state_file_(std::move(state_file)) {}

std::vector<ResumeRecord> ResumeManager::load() const {
    if (!std::filesystem::exists(state_file_)) {
        return {};
    }

    const auto text = read_text_file(state_file_);
    std::vector<ResumeRecord> records;
    const std::regex object_pattern{"\\{[^{}]*\\}"};
    for (std::sregex_iterator it{text.begin(), text.end(), object_pattern}, end; it != end; ++it) {
        const auto object = it->str();
        if (object.find("\"relative_path\"") == std::string::npos) {
            continue;
        }

        ResumeRecord record;
        record.relative_path = require_string_field(object, "relative_path");
        record.size = require_u64_field(object, "size");
        record.sha256 = require_string_field(object, "sha256");
        record.offset = require_u64_field(object, "offset");
        if (record.relative_path.empty() || record.sha256.empty()) {
            throw std::runtime_error("invalid resume state file");
        }
        records.push_back(std::move(record));
    }

    return records;
}

void ResumeManager::save(const std::vector<ResumeRecord>& records) const {
    const auto parent = state_file_.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream output(state_file_, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to open resume state file for writing: " + state_file_.string());
    }

    output << "{\n  \"records\": [\n";
    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto& record = records[i];
        output << "    {\"relative_path\":\"" << escape_json_string(record.relative_path)
               << "\",\"size\":" << record.size
               << ",\"sha256\":\"" << escape_json_string(record.sha256)
               << "\",\"offset\":" << record.offset << "}";
        if (i + 1 != records.size()) {
            output << ',';
        }
        output << '\n';
    }
    output << "  ]\n}\n";

    if (!output) {
        throw std::runtime_error("failed to write resume state file: " + state_file_.string());
    }
}

std::optional<std::uint64_t> ResumeManager::find_offset(const std::string& relative_path,
                                                        std::uint64_t size,
                                                        const std::string& sha256) const {
    for (const auto& record : load()) {
        if (same_identity(record, relative_path, size, sha256)) {
            if (record.offset > size) {
                return 0;
            }
            return record.offset;
        }
    }
    return std::nullopt;
}

void ResumeManager::update_offset(const std::string& relative_path,
                                  std::uint64_t size,
                                  const std::string& sha256,
                                  std::uint64_t offset) const {
    if (relative_path.empty() || sha256.empty()) {
        throw std::runtime_error("invalid resume record identity");
    }
    if (offset > size) {
        throw std::runtime_error("resume offset exceeds file size");
    }

    auto records = load();
    for (auto& record : records) {
        if (same_identity(record, relative_path, size, sha256)) {
            record.offset = offset;
            save(records);
            return;
        }
    }

    records.push_back(ResumeRecord{relative_path, size, sha256, offset});
    save(records);
}

void ResumeManager::clear(const std::string& relative_path,
                          std::uint64_t size,
                          const std::string& sha256) const {
    auto records = load();
    std::vector<ResumeRecord> kept;
    kept.reserve(records.size());
    for (auto& record : records) {
        if (!same_identity(record, relative_path, size, sha256)) {
            kept.push_back(std::move(record));
        }
    }
    save(kept);
}

} // namespace beamdrop::transfer