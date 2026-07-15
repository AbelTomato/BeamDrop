#pragma once

#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

namespace beamdrop::network {

class TcpConnection {
public:
    using NativeHandle = std::intptr_t;

    TcpConnection() = default;
    explicit TcpConnection(NativeHandle handle);
    ~TcpConnection();

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    // Interrupt pending reads/writes and release the socket. This is safe to
    // call from a service-control thread while another thread is blocked in
    // read_exact().
    void shutdown() noexcept;
    void close() noexcept;

    void write_all(std::span<const std::uint8_t> bytes) const;
    [[nodiscard]] std::vector<std::uint8_t> read_exact(std::size_t size) const;

private:
    std::atomic<NativeHandle> handle_{-1};
};

} // namespace beamdrop::network
