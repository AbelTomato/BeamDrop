#include "beamdrop/transfer/ResumeAckCodec.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using beamdrop::transfer::ResumeAck;
using beamdrop::transfer::ResumeAckCodec;

namespace {

std::vector<std::uint8_t> bytes(std::string text) {
    return {text.begin(), text.end()};
}

void assert_rejected(const std::string& payload) {
    bool rejected = false;
    try {
        (void)ResumeAckCodec::decode(bytes(payload));
    } catch (const std::runtime_error&) {
        rejected = true;
    }

    assert(rejected);
}

void test_roundtrip() {
    const ResumeAck input{123};
    const auto payload = ResumeAckCodec::encode(input);
    const std::string text{payload.begin(), payload.end()};
    assert(text == "{\"offset\":123}");

    const auto output = ResumeAckCodec::decode(payload);
    assert(output.offset == input.offset);
}

void test_decode_json_payload() {
    const auto output = ResumeAckCodec::decode(bytes("{\n  \"offset\": 123\n}\n"));
    assert(output.offset == 123);
}

void test_decode_max_u64() {
    const auto output = ResumeAckCodec::decode(bytes("{\"offset\":18446744073709551615}"));
    assert(output.offset == UINT64_MAX);
}

void test_reject_missing_offset() {
    assert_rejected("{}");
}

void test_reject_non_numeric_offset() {
    assert_rejected("{\"offset\":\"abc\"}");
}

void test_reject_negative_offset() {
    assert_rejected("{\"offset\":-1}");
}

void test_reject_trailing_garbage_offset() {
    assert_rejected("{\"offset\":123abc}");
}

void test_reject_trailing_garbage_payload() {
    assert_rejected("{\"offset\":123}abc");
}

void test_reject_u64_overflow() {
    assert_rejected("{\"offset\":18446744073709551616}");
}

} // namespace

int main() {
    test_roundtrip();
    test_decode_json_payload();
    test_decode_max_u64();
    test_reject_missing_offset();
    test_reject_non_numeric_offset();
    test_reject_negative_offset();
    test_reject_trailing_garbage_offset();
    test_reject_trailing_garbage_payload();
    test_reject_u64_overflow();

    std::cout << "resume_ack_codec_tests passed\n";
    return 0;
}