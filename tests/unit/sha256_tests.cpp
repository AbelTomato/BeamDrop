#include "beamdrop/utils/Sha256.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

void test_empty() {
    assert(beamdrop::utils::sha256_hex({}) ==
           "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

void test_abc() {
    assert(beamdrop::utils::sha256_hex(bytes("abc")) ==
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

void test_incremental_abc() {
    beamdrop::utils::Sha256 sha256;
    const auto a = bytes("a");
    const auto b = bytes("b");
    const auto c = bytes("c");
    sha256.update(a);
    sha256.update(b);
    sha256.update(c);

    assert(sha256.final_hex() ==
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

void test_file_hash() {
    const auto path = std::filesystem::temp_directory_path() / "beamdrop_sha256_test.txt";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "abc";
    }

    assert(beamdrop::utils::sha256_file(path, 2) ==
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    std::filesystem::remove(path);
}

} // namespace

int main() {
    test_empty();
    test_abc();
    test_incremental_abc();
    test_file_hash();
    std::cout << "sha256_tests passed\n";
    return 0;
}
