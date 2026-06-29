#include "beamdrop/filesystem/FileScanner.hpp"
#include "beamdrop/filesystem/FileUtils.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <vector>

int main() {
    const auto base_dir = std::filesystem::temp_directory_path() / "beamdrop_file_scanner_test";
    const auto root_dir = base_dir / "root";
    const auto file_a = root_dir / "a.txt";
    const auto file_b = root_dir / "nested" / "b.txt";

    std::filesystem::remove_all(base_dir);
    std::filesystem::create_directories(root_dir / "nested");

    beamdrop::filesystem::write_file(file_a, std::vector<std::uint8_t>{'a'});
    beamdrop::filesystem::write_file(file_b, std::vector<std::uint8_t>{'b', 'b'});

    const auto single_entries = beamdrop::filesystem::scan_files(file_a);
    assert(single_entries.size() == 1);
    assert(single_entries[0].source_path == file_a);
    assert(single_entries[0].relative_path == "a.txt");
    assert(single_entries[0].size == 1);

    const auto directory_entries = beamdrop::filesystem::scan_files(root_dir);
    assert(directory_entries.size() == 2);
    assert(directory_entries[0].source_path == file_a);
    assert(directory_entries[0].relative_path == "a.txt");
    assert(directory_entries[0].size == 1);
    assert(directory_entries[1].source_path == file_b);
    assert(directory_entries[1].relative_path == "nested/b.txt");
    assert(directory_entries[1].size == 2);

    std::filesystem::remove_all(base_dir);

    std::cout << "file_scanner_tests passed\n";
    return 0;
}