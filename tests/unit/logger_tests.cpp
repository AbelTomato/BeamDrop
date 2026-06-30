#include "beamdrop/logger/Logger.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    const auto base_dir = std::filesystem::temp_directory_path() / "beamdrop_logger_test";
    const auto log_file = base_dir / "logs" / "beamdrop.log";

    std::filesystem::remove_all(base_dir);

    const beamdrop::logger::Logger logger{log_file};
    logger.info("transfer started");
    logger.error("transfer failed");

    std::ifstream input(log_file);
    assert(input);

    std::ostringstream content_stream;
    content_stream << input.rdbuf();
    const auto content = content_stream.str();

    assert(content.find("[INFO] transfer started") != std::string::npos);
    assert(content.find("[ERROR] transfer failed") != std::string::npos);

    input.close();
    std::filesystem::remove_all(base_dir);

    std::cout << "logger_tests passed\n";
    return 0;
}