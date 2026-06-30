#include "beamdrop/logger/Logger.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace beamdrop::logger {
namespace {

std::string current_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};
#ifdef _WIN32
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif

    std::ostringstream output;
    output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

} // namespace

Logger::Logger(std::filesystem::path log_file) : log_file_(std::move(log_file)) {}

void Logger::info(const std::string& message) const { write("INFO", message); }

void Logger::error(const std::string& message) const { write("ERROR", message); }

void Logger::write(const std::string& level, const std::string& message) const {
    const auto parent = log_file_.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream output(log_file_, std::ios::app);
    if (!output) {
        throw std::runtime_error("failed to open log file: " + log_file_.string());
    }

    output << current_timestamp() << " [" << level << "] " << message << '\n';
}

} // namespace beamdrop::logger