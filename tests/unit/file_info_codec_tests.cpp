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
    const std::string text{payload.begin(), payload.end()};
    assert(text.find("\"relative_path\": \"nested/hello.txt\"") != std::string::npos);
    assert(text.find("\"size\": 12345") != std::string::npos);
    assert(text.find("\"sha256\": \"abc123\"") != std::string::npos);

    const auto output = FileInfoCodec::decode(payload);

    assert(output.relative_path == input.relative_path);
    assert(output.size == input.size);
    assert(output.sha256 == input.sha256);
}

void test_decode_json_payload() {
    const auto output = FileInfoCodec::decode(bytes(
        "{\n"
        "  \"relative_path\": \"nested/hello.txt\",\n"
        "  \"size\": 12,\n"
        "  \"sha256\": \"abc123\"\n"
        "}\n"));

    assert(output.relative_path == "nested/hello.txt");
    assert(output.size == 12);
    assert(output.sha256 == "abc123");
}

void test_reject_empty_path() {
    bool rejected = false;
    try {
        (void)FileInfoCodec::decode(bytes(
            "{\"relative_path\":\"\",\"size\":10,\"sha256\":\"abc123\"}"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

void test_reject_missing_size() {
    bool rejected = false;
    try {
        (void)FileInfoCodec::decode(bytes(
            "{\"relative_path\":\"nested/hello.txt\",\"sha256\":\"abc123\"}"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

void test_reject_invalid_size() {
    bool rejected = false;
    try {
        (void)FileInfoCodec::decode(bytes(
            "{\"relative_path\":\"nested/hello.txt\",\"size\":\"abc\",\"sha256\":\"abc123\"}"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

void test_reject_trailing_garbage_size() {
    bool rejected = false;
    try {
        (void)FileInfoCodec::decode(bytes(
            "{\"relative_path\":\"nested/hello.txt\",\"size\":12abc,\"sha256\":\"abc123\"}"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

void test_reject_missing_sha256() {
    bool rejected = false;
    try {
        (void)FileInfoCodec::decode(bytes(
            "{\"relative_path\":\"nested/hello.txt\",\"size\":12}"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

void test_reject_legacy_line_payload() {
    bool rejected = false;
    try {
        (void)FileInfoCodec::decode(bytes("nested/hello.txt\n12\nabc123\n"));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

} // namespace

int main() {
    test_roundtrip();
    test_decode_json_payload();
    test_reject_empty_path();
    test_reject_missing_size();
    test_reject_invalid_size();
    test_reject_trailing_garbage_size();
    test_reject_missing_sha256();
    test_reject_legacy_line_payload();

    std::cout << "file_info_codec_tests passed\n";
    return 0;
}