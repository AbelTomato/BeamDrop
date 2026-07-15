#include "beamdrop/transfer/TransferManifest.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool decode_fails(const std::string& text) {
    try {
        const std::vector<std::uint8_t> payload{text.begin(), text.end()};
        (void)beamdrop::transfer::TransferManifestCodec::decode(payload);
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

} // namespace

int main() {
    try {
        const beamdrop::transfer::TransferManifest manifest{3, 42, 4};
        const auto payload = beamdrop::transfer::TransferManifestCodec::encode(manifest);
        const auto decoded = beamdrop::transfer::TransferManifestCodec::decode(payload);

        require(decoded.file_count == 3, "file_count should round-trip");
        require(decoded.total_bytes == 42, "total_bytes should round-trip");
        require(decoded.directory_count == 4, "directory_count should round-trip");

        require(decode_fails("3"), "plain text file count must be rejected");
        require(decode_fails("beamdrop-manifest-v1\nfile_count=1\ntotal_bytes=2\ndirectory_count=0\n"),
                "legacy manifest version must be rejected");
        require(decode_fails("beamdrop-manifest-v2\nfile_count=1abc\ntotal_bytes=2\ndirectory_count=0\n"),
                "invalid file_count must be rejected");
        require(decode_fails("beamdrop-manifest-v2\nfile_count=1\ntotal_bytes=2abc\ndirectory_count=0\n"),
                "invalid total_bytes must be rejected");
        require(decode_fails("beamdrop-manifest-v2\nfile_count=1\ntotal_bytes=2\ndirectory_count=bad\n"),
                "invalid directory_count must be rejected");

        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}