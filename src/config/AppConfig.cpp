#include "beamdrop/config/AppConfig.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace beamdrop::config {
namespace {

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open config file: " + path.string());
    }

    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

std::string object_text(const std::string& json, const std::string& name) {
    const std::regex pattern{'"' + name + R"("\s*:\s*\{([^}]*)\})"};
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        return {};
    }
    return match[1].str();
}

void load_string_field(const std::string& object,
                       const std::string& name,
                       std::string& output) {
    const std::regex pattern{"\\\"" + name + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\""};
    std::smatch match;
    if (std::regex_search(object, match, pattern)) {
        output = match[1].str();
    }
}

void load_path_field(const std::string& object,
                     const std::string& name,
                     std::filesystem::path& output) {
    std::string text = output.string();
    load_string_field(object, name, text);
    output = text;
}

void load_bool_field(const std::string& object, const std::string& name, bool& output) {
    const std::regex pattern{'"' + name + R"("\s*:\s*(true|false))"};
    std::smatch match;
    if (std::regex_search(object, match, pattern)) {
        output = match[1].str() == "true";
    }
}

void load_uint16_field(const std::string& object, const std::string& name, std::uint16_t& output) {
    const std::regex pattern{'"' + name + R"("\s*:\s*([0-9]+))"};
    std::smatch match;
    if (!std::regex_search(object, match, pattern)) {
        return;
    }

    const auto value = std::stoul(match[1].str());
    if (value > 65535) {
        throw std::runtime_error("config field out of range: " + name);
    }
    output = static_cast<std::uint16_t>(value);
}

void load_size_field(const std::string& object, const std::string& name, std::size_t& output) {
    const std::regex pattern{'"' + name + R"("\s*:\s*([0-9]+))"};
    std::smatch match;
    if (!std::regex_search(object, match, pattern)) {
        return;
    }

    output = static_cast<std::size_t>(std::stoull(match[1].str()));
    if (output == 0) {
        throw std::runtime_error("config field must be greater than zero: " + name);
    }
}

} // namespace

AppConfig default_config() {
    return AppConfig{};
}

AppConfig load_config(const std::filesystem::path& path) {
    auto config = default_config();
    const auto json = read_text_file(path);

    const auto server = object_text(json, "server");
    load_string_field(server, "host", config.server.host);
    load_uint16_field(server, "port", config.server.port);
    load_path_field(server, "save_dir", config.server.save_dir);

    const auto transfer = object_text(json, "transfer");
    load_size_field(transfer, "chunk_size", config.transfer.chunk_size);
    load_bool_field(transfer, "enable_resume", config.transfer.enable_resume);
    load_bool_field(transfer, "verify_sha256", config.transfer.verify_sha256);
    load_path_field(transfer, "state_file", config.transfer.state_file);

    const auto log = object_text(json, "log");
    load_string_field(log, "level", config.log.level);
    load_path_field(log, "file", config.log.file);

    return config;
}

std::string format_config(const AppConfig& config) {
    std::ostringstream output;
    output << "{\n"
           << "  \"server\": {\n"
           << "    \"host\": \"" << config.server.host << "\",\n"
           << "    \"port\": " << config.server.port << ",\n"
           << "    \"save_dir\": \"" << config.server.save_dir.string() << "\"\n"
           << "  },\n"
           << "  \"transfer\": {\n"
           << "    \"chunk_size\": " << config.transfer.chunk_size << ",\n"
           << "    \"enable_resume\": " << (config.transfer.enable_resume ? "true" : "false") << ",\n"
           << "    \"verify_sha256\": " << (config.transfer.verify_sha256 ? "true" : "false") << ",\n"
           << "    \"state_file\": \"" << config.transfer.state_file.string() << "\"\n"
           << "  },\n"
           << "  \"log\": {\n"
           << "    \"level\": \"" << config.log.level << "\",\n"
           << "    \"file\": \"" << config.log.file.string() << "\"\n"
           << "  }\n"
           << "}\n";
    return output.str();
}

void write_config(const std::filesystem::path& path, const AppConfig& config) {
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to open config file for writing: " + path.string());
    }

    output << format_config(config);
    if (!output) {
        throw std::runtime_error("failed to write config file: " + path.string());
    }
}

} // namespace beamdrop::config