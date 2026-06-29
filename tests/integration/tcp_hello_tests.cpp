#include "beamdrop/network/TcpClient.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include "beamdrop/protocol/PacketIO.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <thread>
#include <vector>

using beamdrop::network::TcpClient;
using beamdrop::network::TcpConnection;
using beamdrop::network::TcpServer;
using beamdrop::protocol::Packet;
using beamdrop::protocol::PacketType;

int main() {
    constexpr std::uint16_t port = 19090;
    std::exception_ptr server_error;
    Packet received_by_server;

    std::thread server_thread([&] {
        try {
            TcpServer server{"127.0.0.1", port};
            auto connection = server.accept_one();
            received_by_server = beamdrop::protocol::read_packet(connection);

            Packet response;
            response.header.type = PacketType::Hello;
            response.payload = {'O', 'K'};
            beamdrop::protocol::write_packet(connection, response);
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    TcpClient client;
    auto connection = client.connect("127.0.0.1", port);

    Packet hello;
    hello.header.type = PacketType::Hello;
    hello.payload = {'H', 'I'};
    beamdrop::protocol::write_packet(connection, hello);

    const auto response = beamdrop::protocol::read_packet(connection);
    server_thread.join();

    if (server_error) {
        std::rethrow_exception(server_error);
    }

    assert(received_by_server.header.type == PacketType::Hello);
    assert(received_by_server.payload == hello.payload);
    assert(response.header.type == PacketType::Hello);
    assert(response.payload == std::vector<std::uint8_t>({'O', 'K'}));

    std::cout << "tcp_hello_tests passed\n";
    return 0;
}
