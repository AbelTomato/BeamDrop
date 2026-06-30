#include "beamdrop/config/AppConfig.hpp"
#include "beamdrop/filesystem/FileScanner.hpp"
#include "beamdrop/logger/Logger.hpp"
#include "beamdrop/network/TcpClient.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include "beamdrop/protocol/Packet.hpp"
#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/Progress.hpp"
#include "beamdrop/transfer/Receiver.hpp"
#include "beamdrop/transfer/Sender.hpp"
#include "beamdrop/transfer/TransferManifest.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_usage() {
    std::cout << "BeamDrop / 邻光\n"
              << "\n"
              << "Usage:\n"
              << "  beamdrop serve [--config config/beamdrop.example.json] [--host 0.0.0.0] [--port 9090] [--save-dir ./received] [--log-file ./logs/beamdrop.log]\n"
              << "  beamdrop send <path...> --to <host:port> [--config config/beamdrop.example.json] [--chunk-size 1048576] [--log-file ./logs/beamdrop.log]\n"
              << "  beamdrop config init|show\n"
              << "\n";
}

std::uint16_t parse_port(const std::string& text) {
    const auto value = std::stoul(text);
    if (value > 65535) {
        throw std::runtime_error("port out of range: " + text);
    }
    return static_cast<std::uint16_t>(value);
}

struct Endpoint {
    std::string host;
    std::uint16_t port;
};

Endpoint parse_endpoint(const std::string& text) {
    const auto separator = text.rfind(':');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= text.size()) {
        throw std::runtime_error("--to must use host:port format");
    }

    return Endpoint{text.substr(0, separator), parse_port(text.substr(separator + 1))};
}

std::uint64_t total_entry_bytes(const std::vector<beamdrop::filesystem::FileEntry>& entries) {
    std::uint64_t total = 0;
    for (const auto& entry : entries) {
        total += entry.size;
    }
    return total;
}

void print_progress(const beamdrop::transfer::ProgressEvent& event) {
    const auto verb = event.direction == beamdrop::transfer::ProgressDirection::Send ? "send" : "receive";
    std::cout << "beamdrop " << verb << " [" << event.file_index << '/' << event.file_count << "] "
              << event.relative_path << ' ' << event.current_file_bytes << '/'
              << event.current_file_total_bytes << " bytes";
    if (event.file_complete) {
        std::cout << " done";
    }
    std::cout << '\n';
}

std::string paths_text(const std::vector<std::filesystem::path>& paths) {
    std::ostringstream output;
    for (std::size_t index = 0; index < paths.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << paths[index].string();
    }
    return output.str();
}

struct ServeOptions {
    std::string host = "0.0.0.0";
    std::uint16_t port = 9090;
    std::filesystem::path save_dir = "received";
    std::filesystem::path log_file = "logs/beamdrop.log";
};

ServeOptions parse_serve_options(int argc, char* argv[]) {
    auto config = beamdrop::config::default_config();
    ServeOptions options;
    for (int index = 2; index < argc; ++index) {
        const std::string_view arg{argv[index]};
        if (arg == "--config" && index + 1 < argc) {
            config = beamdrop::config::load_config(argv[++index]);
            options.host = config.server.host;
            options.port = config.server.port;
            options.save_dir = config.server.save_dir;
            options.log_file = config.log.file;
        } else if (arg == "--host" && index + 1 < argc) {
            options.host = argv[++index];
        } else if (arg == "--port" && index + 1 < argc) {
            options.port = parse_port(argv[++index]);
        } else if (arg == "--save-dir" && index + 1 < argc) {
            options.save_dir = argv[++index];
        } else if (arg == "--log-file" && index + 1 < argc) {
            options.log_file = argv[++index];
        } else {
            throw std::runtime_error("unknown or incomplete serve argument: " + std::string{arg});
        }
    }
    return options;
}

struct SendOptions {
    std::vector<std::filesystem::path> paths;
    Endpoint endpoint{"", 0};
    bool has_endpoint = false;
    std::filesystem::path log_file = "logs/beamdrop.log";
    std::size_t chunk_size = 1024 * 1024;
};

struct ConfigOptions {
    std::string action;
    std::filesystem::path path = "config/beamdrop.json";
};

SendOptions parse_send_options(int argc, char* argv[]) {
    auto config = beamdrop::config::default_config();
    SendOptions options;
    for (int index = 2; index < argc; ++index) {
        const std::string_view arg{argv[index]};
        if (arg == "--config" && index + 1 < argc) {
            config = beamdrop::config::load_config(argv[++index]);
            options.log_file = config.log.file;
            options.chunk_size = config.transfer.chunk_size;
        } else if (arg == "--to" && index + 1 < argc) {
            options.endpoint = parse_endpoint(argv[++index]);
            options.has_endpoint = true;
        } else if (arg == "--log-file" && index + 1 < argc) {
            options.log_file = argv[++index];
        } else if (arg == "--chunk-size" && index + 1 < argc) {
            options.chunk_size = static_cast<std::size_t>(std::stoull(argv[++index]));
            if (options.chunk_size == 0) {
                throw std::runtime_error("--chunk-size must be greater than zero");
            }
        } else if (!arg.empty() && arg.front() == '-') {
            throw std::runtime_error("unknown or incomplete send argument: " + std::string{arg});
        } else {
            options.paths.emplace_back(argv[index]);
        }
    }

    if (options.paths.empty()) {
        throw std::runtime_error("send requires at least one path");
    }
    if (!options.has_endpoint) {
        throw std::runtime_error("send requires --to <host:port>");
    }
    return options;
}

ConfigOptions parse_config_options(int argc, char* argv[]) {
    if (argc < 3) {
        throw std::runtime_error("config requires init or show");
    }

    ConfigOptions options;
    options.action = argv[2];
    for (int index = 3; index < argc; ++index) {
        const std::string_view arg{argv[index]};
        if ((arg == "--path" || arg == "--config") && index + 1 < argc) {
            options.path = argv[++index];
        } else {
            throw std::runtime_error("unknown or incomplete config argument: " + std::string{arg});
        }
    }

    if (options.action != "init" && options.action != "show") {
        throw std::runtime_error("config action must be init or show");
    }
    return options;
}

int run_serve(int argc, char* argv[]) {
    const auto options = parse_serve_options(argc, argv);
    const beamdrop::logger::Logger logger{options.log_file};
    logger.info("serve starting host=" + options.host + " port=" + std::to_string(options.port)
                + " save_dir=" + options.save_dir.string());

    try {
        beamdrop::network::TcpServer server{options.host, options.port};

        std::cout << "beamdrop serve listening on " << options.host << ':' << options.port
                  << ", save-dir=" << options.save_dir.string() << '\n';

        auto connection = server.accept_one();
        logger.info("serve accepted connection");
        const auto hello = beamdrop::protocol::read_packet(connection);
        if (hello.header.type != beamdrop::protocol::PacketType::Hello) {
            throw std::runtime_error("expected HELLO packet with transfer manifest");
        }

        const auto manifest = beamdrop::transfer::TransferManifestCodec::decode(hello.payload);
        const auto file_count = manifest.file_count;
        logger.info("serve receiving file_count=" + std::to_string(file_count)
                    + " total_bytes=" + std::to_string(manifest.total_bytes));
        beamdrop::transfer::Receiver receiver{connection, print_progress};
        receiver.receive_files(options.save_dir, static_cast<std::size_t>(file_count));

        const auto finish = beamdrop::protocol::read_packet(connection);
        if (finish.header.type != beamdrop::protocol::PacketType::Finish) {
            throw std::runtime_error("expected FINISH packet");
        }

        std::cout << "beamdrop serve received " << file_count << " file(s)\n";
        logger.info("serve completed file_count=" + std::to_string(file_count));
        return 0;
    } catch (const std::exception& error) {
        logger.error(std::string{"serve failed: "} + error.what());
        throw;
    }
}

int run_send(int argc, char* argv[]) {
    const auto options = parse_send_options(argc, argv);
    const beamdrop::logger::Logger logger{options.log_file};
    logger.info("send starting endpoint=" + options.endpoint.host + ':'
                + std::to_string(options.endpoint.port) + " paths=" + paths_text(options.paths));

    try {
        std::vector<beamdrop::filesystem::FileEntry> entries;
        for (const auto& path : options.paths) {
            auto scanned = beamdrop::filesystem::scan_files(path);
            entries.insert(entries.end(), scanned.begin(), scanned.end());
        }
        logger.info("send scanned file_count=" + std::to_string(entries.size()));

        beamdrop::network::TcpClient client;
        auto connection = client.connect(options.endpoint.host, options.endpoint.port);
        logger.info("send connected endpoint=" + options.endpoint.host + ':'
                    + std::to_string(options.endpoint.port));

        beamdrop::protocol::Packet hello;
        hello.header.type = beamdrop::protocol::PacketType::Hello;
        const beamdrop::transfer::TransferManifest manifest{
            static_cast<std::uint64_t>(entries.size()), total_entry_bytes(entries)};
        logger.info("send manifest file_count=" + std::to_string(manifest.file_count)
                    + " total_bytes=" + std::to_string(manifest.total_bytes));
        hello.payload = beamdrop::transfer::TransferManifestCodec::encode(manifest);
        beamdrop::protocol::write_packet(connection, hello);

        beamdrop::transfer::Sender sender{connection, print_progress, options.chunk_size};
        sender.send_files(entries);

        beamdrop::protocol::Packet finish;
        finish.header.type = beamdrop::protocol::PacketType::Finish;
        beamdrop::protocol::write_packet(connection, finish);

        std::cout << "beamdrop send sent " << entries.size() << " file(s) to "
                  << options.endpoint.host << ':' << options.endpoint.port << '\n';
        logger.info("send completed file_count=" + std::to_string(entries.size()));
        return 0;
    } catch (const std::exception& error) {
        logger.error(std::string{"send failed: "} + error.what());
        throw;
    }
}

int run_config(int argc, char* argv[]) {
    const auto options = parse_config_options(argc, argv);
    if (options.action == "init") {
        beamdrop::config::write_config(options.path, beamdrop::config::default_config());
        std::cout << "beamdrop config initialized " << options.path.string() << '\n';
        return 0;
    }

    const auto config = beamdrop::config::load_config(options.path);
    std::cout << beamdrop::config::format_config(config);
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        print_usage();
        return 0;
    }

    const std::string_view command{argv[1]};

    try {
        if (command == "serve") {
            return run_serve(argc, argv);
        }

        if (command == "send") {
            return run_send(argc, argv);
        }

        if (command == "config") {
            return run_config(argc, argv);
        }

        print_usage();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "beamdrop error: " << error.what() << '\n';
        return 1;
    }
}
