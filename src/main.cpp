#include "beamdrop/filesystem/FileScanner.hpp"
#include "beamdrop/network/TcpClient.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include "beamdrop/protocol/Packet.hpp"
#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/Receiver.hpp"
#include "beamdrop/transfer/Sender.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_usage() {
    std::cout << "BeamDrop / 邻光\n"
              << "\n"
              << "Usage:\n"
              << "  beamdrop serve [--host 0.0.0.0] [--port 9090] [--save-dir ./received]\n"
              << "  beamdrop send <path...> --to <host:port>\n"
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

std::vector<std::uint8_t> text_payload(const std::string& text) {
    return std::vector<std::uint8_t>{text.begin(), text.end()};
}

std::string payload_text(const std::vector<std::uint8_t>& payload) {
    return std::string{payload.begin(), payload.end()};
}

struct ServeOptions {
    std::string host = "0.0.0.0";
    std::uint16_t port = 9090;
    std::filesystem::path save_dir = "received";
};

ServeOptions parse_serve_options(int argc, char* argv[]) {
    ServeOptions options;
    for (int index = 2; index < argc; ++index) {
        const std::string_view arg{argv[index]};
        if (arg == "--host" && index + 1 < argc) {
            options.host = argv[++index];
        } else if (arg == "--port" && index + 1 < argc) {
            options.port = parse_port(argv[++index]);
        } else if (arg == "--save-dir" && index + 1 < argc) {
            options.save_dir = argv[++index];
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
};

SendOptions parse_send_options(int argc, char* argv[]) {
    SendOptions options;
    for (int index = 2; index < argc; ++index) {
        const std::string_view arg{argv[index]};
        if (arg == "--to" && index + 1 < argc) {
            options.endpoint = parse_endpoint(argv[++index]);
            options.has_endpoint = true;
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

int run_serve(int argc, char* argv[]) {
    const auto options = parse_serve_options(argc, argv);
    beamdrop::network::TcpServer server{options.host, options.port};

    std::cout << "beamdrop serve listening on " << options.host << ':' << options.port
              << ", save-dir=" << options.save_dir.string() << '\n';

    auto connection = server.accept_one();
    const auto hello = beamdrop::protocol::read_packet(connection);
    if (hello.header.type != beamdrop::protocol::PacketType::Hello) {
        throw std::runtime_error("expected HELLO packet with file count");
    }

    const auto file_count = std::stoull(payload_text(hello.payload));
    beamdrop::transfer::Receiver receiver{connection};
    receiver.receive_files(options.save_dir, static_cast<std::size_t>(file_count));

    const auto finish = beamdrop::protocol::read_packet(connection);
    if (finish.header.type != beamdrop::protocol::PacketType::Finish) {
        throw std::runtime_error("expected FINISH packet");
    }

    std::cout << "beamdrop serve received " << file_count << " file(s)\n";
    return 0;
}

int run_send(int argc, char* argv[]) {
    const auto options = parse_send_options(argc, argv);

    std::vector<beamdrop::filesystem::FileEntry> entries;
    for (const auto& path : options.paths) {
        auto scanned = beamdrop::filesystem::scan_files(path);
        entries.insert(entries.end(), scanned.begin(), scanned.end());
    }

    beamdrop::network::TcpClient client;
    auto connection = client.connect(options.endpoint.host, options.endpoint.port);

    beamdrop::protocol::Packet hello;
    hello.header.type = beamdrop::protocol::PacketType::Hello;
    hello.payload = text_payload(std::to_string(entries.size()));
    beamdrop::protocol::write_packet(connection, hello);

    beamdrop::transfer::Sender sender{connection};
    sender.send_files(entries);

    beamdrop::protocol::Packet finish;
    finish.header.type = beamdrop::protocol::PacketType::Finish;
    beamdrop::protocol::write_packet(connection, finish);

    std::cout << "beamdrop send sent " << entries.size() << " file(s) to " << options.endpoint.host
              << ':' << options.endpoint.port << '\n';
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
            std::cout << "beamdrop config: implementation pending.\n";
            return 0;
        }

        print_usage();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "beamdrop error: " << error.what() << '\n';
        return 1;
    }
}
