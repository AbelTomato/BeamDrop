#include "beamdrop/app/ReceiveServerService.hpp"
#include "beamdrop/app/ReceiveService.hpp"
#include "beamdrop/app/SendService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/config/AppConfig.hpp"
#include "beamdrop/logger/Logger.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include "beamdrop/transfer/Progress.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_usage() {
    std::cout
        << "BeamDrop / 邻光\n"
        << "\n"
        << "Usage:\n"
        << "  beamdrop serve [--config config/beamdrop.example.json] [--host 0.0.0.0] [--port "
           "9090] [--save-dir ./received] [--log-file ./logs/beamdrop.log]\n"
        << "  beamdrop send <path...> --to <host:port> [--config config/beamdrop.example.json] "
           "[--chunk-size 1048576] [--log-file ./logs/beamdrop.log]\n"
        << "  beamdrop config init|show\n"
        << "\n";
}

std::uint16_t parse_port(const std::string &text) {
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

Endpoint parse_endpoint(const std::string &text) {
    const auto separator = text.rfind(':');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= text.size()) {
        throw std::runtime_error("--to must use host:port format");
    }

    return Endpoint{text.substr(0, separator), parse_port(text.substr(separator + 1))};
}

void print_app_progress(const beamdrop::app::TransferProgress &progress) {
    const auto verb =
        progress.direction == beamdrop::app::TransferProgress::Direction::Send ? "send" : "receive";

    std::cout << "beamdrop " << verb << " [" << progress.file_index << '/' << progress.file_count
              << "] " << progress.relative_path << ' ' << progress.current_file_bytes << '/'
              << progress.current_file_total_bytes << " bytes";
    if (progress.stage == beamdrop::transfer::Stage::TaskCompleted) {
        std::cout << " done";
    }
    std::cout << '\n';
}

std::string paths_text(const std::vector<std::filesystem::path> &paths) {
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
    bool enable_resume = false;
    std::filesystem::path state_file = "transfer_state.json";
    std::filesystem::path log_file = "logs/beamdrop.log";
};

ServeOptions parse_serve_options(int argc, char *argv[]) {
    auto config = beamdrop::config::default_config();
    ServeOptions options;
    for (int index = 2; index < argc; ++index) {
        const std::string_view arg{argv[index]};
        if (arg == "--config" && index + 1 < argc) {
            config = beamdrop::config::load_config(argv[++index]);
            options.host = config.server.host;
            options.port = config.server.port;
            options.save_dir = config.server.save_dir;
            options.enable_resume = config.transfer.enable_resume;
            options.state_file = config.transfer.state_file;
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

SendOptions parse_send_options(int argc, char *argv[]) {
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

ConfigOptions parse_config_options(int argc, char *argv[]) {
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

std::size_t receive_transfer_session(const beamdrop::network::TcpConnection &connection,
                                     const ServeOptions &options,
                                     const std::stop_token stop_token) {
    auto request = beamdrop::app::ReceiveRequest{};
    request.enable_resume = options.enable_resume;
    request.progress_callback = print_app_progress;
    request.save_dir = options.save_dir;
    request.state_file = options.state_file;

    auto service = beamdrop::app::ReceiveService{};
    auto result = service.receive(connection, request, stop_token);

    if (result) {
        return result.value().file_count;
    } else {
        throw std::runtime_error(result.error().message);
    }
}

int run_serve(int argc, char *argv[]) {
    const auto options = parse_serve_options(argc, argv);
    const beamdrop::logger::Logger logger{options.log_file};
    logger.info("serve starting host=" + options.host + " port=" + std::to_string(options.port) +
                " save_dir=" + options.save_dir.string());

    try {
        auto request = beamdrop::app::ReceiveServerRequest{};
        request.host = options.host;
        request.port = options.port;
        request.receive_request.progress_callback = print_app_progress;
        request.receive_request.enable_resume = options.enable_resume;
        request.receive_request.state_file = options.state_file;
        request.receive_request.save_dir = options.save_dir;
        auto server = beamdrop::app::ReceiveServerService{};
        auto result = server.start(request);

        if (!result) {
            throw std::runtime_error(result.error().message);
        }

        std::cout << "beamdrop serve listening on " << options.host << ':' << options.port
                  << ", save-dir=" << options.save_dir.string() << '\n';

        return 0;
    } catch (const std::exception &error) {
        logger.error(std::string{"serve failed: "} + error.what());
        throw;
    }
}

int run_send(int argc, char *argv[]) {
    const auto options = parse_send_options(argc, argv);
    const beamdrop::logger::Logger logger{options.log_file};
    logger.info("send starting endpoint=" + options.endpoint.host + ':' +
                std::to_string(options.endpoint.port) + " paths=" + paths_text(options.paths));

    auto request = beamdrop::app::SendRequest{};
    request.chunk_size = options.chunk_size;
    request.host = options.endpoint.host;
    request.port = options.endpoint.port;
    request.paths = options.paths;
    request.progress_callback = print_app_progress;

    try {
        auto service = beamdrop::app::SendService{};
        auto result = service.send(request);

        if (result) {
            std::cout << "beamdrop send sent " << result.value().file_count << " file(s) to "
                      << options.endpoint.host << ':' << options.endpoint.port << '\n';
            logger.info("send completed file_count=" + std::to_string(result.value().file_count));
            return 0;
        } else {
            throw std::runtime_error(result.error().message);
        }

    } catch (const std::exception &error) {
        logger.error(std::string{"send failed: "} + error.what());
        throw;
    }
}

int run_config(int argc, char *argv[]) {
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

int main(int argc, char *argv[]) {
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
    } catch (const std::exception &error) {
        std::cerr << "beamdrop error: " << error.what() << '\n';
        return 1;
    }
}
