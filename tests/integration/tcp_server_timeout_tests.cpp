#include "beamdrop/network/TcpClient.hpp"
#include "beamdrop/network/TcpConnection.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
int main() {
    using namespace std::chrono_literals;

    std::string host = "127.0.0.1";
    std::uint16_t port = 19102;
    beamdrop::network::TcpServer server{host, port};

    auto start = std::chrono::steady_clock::now();

    auto result = server.accept_one_for(50ms);
    assert(!result);

    auto end = std::chrono::steady_clock::now();
    auto diff = end - start;
    auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
    assert(milli >= 25ms);

    beamdrop::network::TcpClient client;
    beamdrop::network::TcpConnection connection = client.connect(host, port);

    result = server.accept_one_for(1s);
    assert(result);

    return 0;
}