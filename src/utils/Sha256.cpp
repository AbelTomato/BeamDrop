#include "beamdrop/utils/Sha256.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include "beamdrop/transfer/TransferError.hpp"

namespace beamdrop::utils {
namespace {

constexpr std::array<std::uint32_t, 64> k{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
    0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
    0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
    0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

std::uint32_t rotr(std::uint32_t value, std::uint32_t bits) {
    return (value >> bits) | (value << (32U - bits));
}

std::uint32_t load_be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

std::string to_hex(const std::array<std::uint32_t, 8>& state) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto word : state) {
        output << std::setw(8) << word;
    }
    return output.str();
}

} // namespace

Sha256::Sha256()
    : state_{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
             0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U} {}

void Sha256::update(std::span<const std::uint8_t> bytes) {
    if (finalized_) {
        throw std::runtime_error("cannot update finalized SHA256");
    }

    total_bytes_ += bytes.size();
    std::size_t offset = 0;

    if (buffer_size_ > 0) {
        const auto copy_size = std::min(buffer_.size() - buffer_size_, bytes.size());
        std::copy_n(bytes.begin(), copy_size, buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_));
        buffer_size_ += copy_size;
        offset += copy_size;

        if (buffer_size_ == buffer_.size()) {
            process_block(std::span<const std::uint8_t, 64>{buffer_});
            buffer_size_ = 0;
        }
    }

    while (offset + 64U <= bytes.size()) {
        process_block(std::span<const std::uint8_t, 64>{bytes.subspan(offset, 64U)});
        offset += 64U;
    }

    if (offset < bytes.size()) {
        buffer_size_ = bytes.size() - offset;
        std::copy(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end(), buffer_.begin());
    }
}

std::string Sha256::final_hex() {
    if (finalized_) {
        return to_hex(state_);
    }

    const auto bit_length = total_bytes_ * 8U;
    std::array<std::uint8_t, 128> final_blocks{};
    std::copy_n(buffer_.begin(), buffer_size_, final_blocks.begin());
    final_blocks[buffer_size_] = 0x80U;

    const auto final_size = ((buffer_size_ + 1U) <= 56U) ? 64U : 128U;
    for (int shift = 56; shift >= 0; shift -= 8) {
        final_blocks[final_size - 8U + static_cast<std::size_t>((56 - shift) / 8)] =
            static_cast<std::uint8_t>((bit_length >> shift) & 0xFFU);
    }

    process_block(std::span<const std::uint8_t, 64>{final_blocks.data(), 64U});
    if (final_size == 128U) {
        process_block(std::span<const std::uint8_t, 64>{final_blocks.data() + 64U, 64U});
    }

    finalized_ = true;
    return to_hex(state_);
}

void Sha256::process_block(std::span<const std::uint8_t, 64> block) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16U; ++i) {
        w[i] = load_be32(block, i * 4U);
    }
    for (std::size_t i = 16U; i < 64U; ++i) {
        const auto s0 = rotr(w[i - 15U], 7U) ^ rotr(w[i - 15U], 18U) ^ (w[i - 15U] >> 3U);
        const auto s1 = rotr(w[i - 2U], 17U) ^ rotr(w[i - 2U], 19U) ^ (w[i - 2U] >> 10U);
        w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }

    auto a = state_[0];
    auto b = state_[1];
    auto c = state_[2];
    auto d = state_[3];
    auto e = state_[4];
    auto f = state_[5];
    auto g = state_[6];
    auto h = state_[7];

    for (std::size_t i = 0; i < 64U; ++i) {
        const auto s1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
        const auto ch = (e & f) ^ ((~e) & g);
        const auto temp1 = h + s1 + ch + k[i] + w[i];
        const auto s0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
        const auto maj = (a & b) ^ (a & c) ^ (b & c);
        const auto temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

std::string sha256_hex(const std::vector<std::uint8_t>& bytes) {
    Sha256 sha256;
    sha256.update(bytes);
    return sha256.final_hex();
}

std::string sha256_file(const std::filesystem::path& path, std::size_t chunk_size,
                        std::stop_token stop_token) {
    if (chunk_size == 0) {
        throw std::runtime_error("sha256_file chunk_size must be greater than zero");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open file for sha256: " + path.string());
    }

    Sha256 sha256;
    std::vector<std::uint8_t> buffer(chunk_size);
    while (input) {
        if (stop_token.stop_requested()) {
            throw transfer::TransferError{transfer::ErrorCode::Cancelled, "send cancelled"};
        }
        input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const auto read_count = input.gcount();
        if (read_count > 0) {
            sha256.update(std::span<const std::uint8_t>{buffer.data(), static_cast<std::size_t>(read_count)});
        }
    }

    if (!input.eof()) {
        throw std::runtime_error("failed while reading file for sha256: " + path.string());
    }

    return sha256.final_hex();
}

} // namespace beamdrop::utils
