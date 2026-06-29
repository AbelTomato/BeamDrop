#include "beamdrop/transfer/FileInfoCodec.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using beamdrop::transfer::FileInfo;
using beamdrop::transfer::FileInfoCodec;

namespace {

std::vector<std::uint8_t> bytes(std::string text) {
    return {text.begin(), text.end()};
}

void test_roundtrip() {
    const FileInfo input{"nested/hello.txt", 12345, "abc123"};
    const auto payload = FileInfoCodec::encode(input);
    const auto output = FileInfoCodec::decode(payload);

    assert(output.relative_path == input.relative_path);
    assert(output.size == input.size);
    assert(output.sha256 == input.sha256);
}

void test_reject_empty_path() {
    bool rejected = false;
    try {
        (void)FileInfoCodec::decode(bytes("\n10\nabc123\n"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

void test_reject_missing_size() {
    bool rejected = false;
    try {
        (void)FileInfoCodec::decode(bytes("nested/hello.txt\n"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

void test_reject_invalid_size() {
    bool rejected = false;
    try {
        (void)FileInfoCodec::decode(bytes("nested/hello.txt\nabc\nabc123\n"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

void test_reject_trailing_garbage_size() {
    bool rejected = false;
    try {
        (void)FileInfoCodec::decode(bytes("nested/hello.txt\n12abc\nabc123\n"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

void test_reject_missing_sha256() {
    bool rejected = false;
    try {
        (void)FileInfoCodec::decode(bytes("nested/hello.txt\n12\n"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

} // namespace

int main() {
    test_roundtrip();
    test_reject_empty_path();
    test_reject_missing_size();
    test_reject_invalid_size();
    test_reject_trailing_garbage_size();
    test_reject_missing_sha256();

    std::cout << "file_info_codec_tests passed\n";
    return 0;
}