#include "beamdrop/transfer/ResumeManager.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>

using beamdrop::transfer::ResumeManager;

namespace {

void assert_rejected_offset_over_size(const ResumeManager& manager) {
    bool rejected = false;
    try {
        manager.update_offset("a.txt", 10, "sha", 11);
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

} // namespace

int main() {
    const auto base_dir = std::filesystem::temp_directory_path() / "beamdrop_resume_manager_test";
    const auto state_file = base_dir / "nested" / "transfer_state.json";

    std::filesystem::remove_all(base_dir);

    const ResumeManager manager{state_file};
    assert(manager.load().empty());
    assert(!manager.find_offset("a.txt", 10, "sha").has_value());

    manager.update_offset("a.txt", 10, "sha", 4);
    auto records = manager.load();
    assert(records.size() == 1);
    assert(records[0].relative_path == "a.txt");
    assert(records[0].size == 10);
    assert(records[0].sha256 == "sha");
    assert(records[0].offset == 4);
    assert(manager.find_offset("a.txt", 10, "sha") == std::optional<std::uint64_t>{4});
    assert(!manager.find_offset("a.txt", 11, "sha").has_value());
    assert(!manager.find_offset("a.txt", 10, "other").has_value());

    manager.update_offset("a.txt", 10, "sha", 7);
    records = manager.load();
    assert(records.size() == 1);
    assert(records[0].offset == 7);
    assert(manager.find_offset("a.txt", 10, "sha") == std::optional<std::uint64_t>{7});

    assert_rejected_offset_over_size(manager);

    manager.update_offset("nested/quoted\"name.txt", 20, "sha2", 5);
    records = manager.load();
    assert(records.size() == 2);
    assert(manager.find_offset("nested/quoted\"name.txt", 20, "sha2") ==
           std::optional<std::uint64_t>{5});

    manager.clear("a.txt", 10, "sha");
    records = manager.load();
    assert(records.size() == 1);
    assert(!manager.find_offset("a.txt", 10, "sha").has_value());
    assert(manager.find_offset("nested/quoted\"name.txt", 20, "sha2") ==
           std::optional<std::uint64_t>{5});

    std::ofstream output(state_file, std::ios::trunc);
    output << R"({
  "records": [
    {"relative_path":"too-large.bin","size":10,"sha256":"bad","offset":11}
  ]
})";
    output.close();
    assert(manager.find_offset("too-large.bin", 10, "bad") == std::optional<std::uint64_t>{0});

    manager.clear("too-large.bin", 10, "bad");
    assert(manager.load().empty());

    std::filesystem::remove_all(base_dir);

    std::cout << "resume_manager_tests passed\n";
    return 0;
}