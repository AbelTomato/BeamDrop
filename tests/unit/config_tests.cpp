#include "beamdrop/config/AppConfig.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    const auto base_dir = std::filesystem::temp_directory_path() / "beamdrop_config_test";
    const auto config_file = base_dir / "beamdrop.json";

    std::filesystem::remove_all(base_dir);
    std::filesystem::create_directories(base_dir);

    std::ofstream output(config_file);
    output << R"({
  "server": { "host": "127.0.0.1", "port": 19090, "save_dir": "out" },
  "transfer": { "chunk_size": 7, "enable_resume": true, "verify_sha256": false, "state_file": "state.json" },
  "log": { "level": "debug", "file": "logs/test.log" }
})";
    output.close();

    const auto config = beamdrop::config::load_config(config_file);
    assert(config.server.host == "127.0.0.1");
    assert(config.server.port == 19090);
    assert(config.server.save_dir == "out");
    assert(config.transfer.chunk_size == 7);
    assert(config.transfer.enable_resume);
    assert(!config.transfer.verify_sha256);
    assert(config.transfer.state_file == "state.json");
    assert(config.log.level == "debug");
    assert(config.log.file == "logs/test.log");

    const auto generated_file = base_dir / "nested" / "generated.json";
    beamdrop::config::write_config(generated_file, beamdrop::config::default_config());
    const auto generated = beamdrop::config::load_config(generated_file);
    assert(generated.server.host == "0.0.0.0");
    assert(generated.server.port == 9090);
    assert(generated.server.save_dir == "received");
    assert(generated.transfer.chunk_size == 1024 * 1024);
    assert(!generated.transfer.enable_resume);
    assert(generated.transfer.verify_sha256);
    assert(generated.transfer.state_file == "transfer_state.json");
    assert(generated.log.level == "info");
    assert(generated.log.file == "logs/beamdrop.log");

    const auto formatted = beamdrop::config::format_config(generated);
    assert(formatted.find("\"server\"") != std::string::npos);
    assert(formatted.find("\"chunk_size\": 1048576") != std::string::npos);

    std::filesystem::remove_all(base_dir);

    std::cout << "config_tests passed\n";
    return 0;
}