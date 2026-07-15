#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/transfer/Progress.hpp"

#include <cassert>
#include <cstdint>

void test_to_app_progress() {
    auto direction = beamdrop::transfer::ProgressDirection::Receive;
    std::string relative_path = "./data.json";
    std::uint64_t current_file_bytes = 1456;
    std::uint64_t current_total_file_bytes = 1254353459;
    std::size_t file_index = 3;
    std::size_t file_count = 10;

    auto event = beamdrop::transfer::ProgressEvent{direction,          relative_path,
                                                   current_file_bytes, current_total_file_bytes,
                                                   file_index,         file_count};

    auto progress = beamdrop::app::to_app_progress(event);

    assert(progress.direction == beamdrop::app::TransferProgress::Direction::Receive);
    assert(progress.relative_path == relative_path);
    assert(progress.current_file_bytes == current_file_bytes);
    assert(progress.current_file_total_bytes == current_total_file_bytes);
    assert(progress.file_index == file_index);
    assert(progress.file_count == file_count);
}

int main() {
    test_to_app_progress();

    return 0;
}