#include "beamdrop/transfer/DirectoryInfoCodec.hpp"
#include "beamdrop/transfer/TransferError.hpp"

namespace beamdrop::transfer {

std::vector<std::uint8_t> DirectoryInfoCodec::encode(const std::string& relative_path) {
    if (relative_path.empty()) {
        throw TransferError{ErrorCode::InvalidPayload, "directory path must not be empty"};
    }
    return {relative_path.begin(), relative_path.end()};
}

std::string DirectoryInfoCodec::decode(const std::vector<std::uint8_t>& payload) {
    const std::string relative_path{payload.begin(), payload.end()};
    if (relative_path.empty()) {
        throw TransferError{ErrorCode::InvalidPayload, "directory path must not be empty"};
    }
    return relative_path;
}

} // namespace beamdrop::transfer