#pragma once

#include <filesystem>
#include <string>

namespace beamdrop::logger {

class Logger {
public:
    explicit Logger(std::filesystem::path log_file);

    void info(const std::string& message) const;
    void error(const std::string& message) const;

private:
    void write(const std::string& level, const std::string& message) const;

    std::filesystem::path log_file_;
};

} // namespace beamdrop::logger