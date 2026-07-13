#include "beamdrop/filesystem/FileUtils.hpp"
#include "beamdrop/network/TcpClient.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include "beamdrop/protocol/Packet.hpp"
#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/Receiver.hpp"
#include "beamdrop/transfer/Sender.hpp"
#include "beamdrop/transfer/TransferManifest.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using beamdrop::network::TcpClient;
using beamdrop::network::TcpServer;
using beamdrop::protocol::Packet;
using beamdrop::protocol::PacketType;
using beamdrop::transfer::Receiver;
using beamdrop::transfer::Sender;
using beamdrop::transfer::TransferManifest;
using beamdrop::transfer::TransferManifestCodec;

namespace {

void send_one_file(const std::filesystem::path &source_path, const std::string &relative_path,
                   std::uint16_t port) {
    TcpClient client;
    auto connection = client.connect("127.0.0.1", port);

    const auto size = static_cast<std::uint64_t>(std::filesystem::file_size(source_path));
    Packet hello;
    hello.header.type = PacketType::Hello;
    hello.payload = TransferManifestCodec::encode(TransferManifest{1, size});
    beamdrop::protocol::write_packet(connection, hello);

    Sender sender{connection, {}, 4};
    sender.send_file(source_path, relative_path);

    Packet finish;
    finish.header.type = PacketType::Finish;
    beamdrop::protocol::write_packet(connection, finish);
}

void receive_one_session(const beamdrop::network::TcpConnection &connection,
                         const std::filesystem::path &receive_dir,
                         const std::filesystem::path &state_file) {
    const auto hello = beamdrop::protocol::read_packet(connection);
    assert(hello.header.type == PacketType::Hello);
    const auto manifest = TransferManifestCodec::decode(hello.payload);
    assert(manifest.file_count == 1);

    Receiver receiver{connection, {}, true, state_file};
    receiver.receive_task(receive_dir, static_cast<std::size_t>(manifest.file_count));

    const auto finish = beamdrop::protocol::read_packet(connection);
    assert(finish.header.type == PacketType::Finish);
}

} // namespace

int main() {
    using namespace std::chrono_literals;

    constexpr std::uint16_t port = 19094;

    const auto base_dir = std::filesystem::temp_directory_path() / "beamdrop_persistent_serve_test";
    const auto send_dir = base_dir / "send";
    const auto receive_dir = base_dir / "received";
    const auto state_file = base_dir / "transfer_state.json";

    const auto source_a = send_dir / "a.txt";
    const auto source_b = send_dir / "b.txt";
    const auto received_a = receive_dir / "first" / "a.txt";
    const auto received_b = receive_dir / "second" / "b.txt";

    const std::vector<std::uint8_t> content_a = {'f', 'i', 'r', 's', 't', '\n'};
    const std::vector<std::uint8_t> content_b = {'s', 'e', 'c', 'o', 'n', 'd', '\n'};

    std::filesystem::remove_all(base_dir);
    beamdrop::filesystem::write_file(source_a, content_a);
    beamdrop::filesystem::write_file(source_b, content_b);

    std::exception_ptr server_error;
    std::thread server_thread([&] {
        try {
            TcpServer server{"127.0.0.1", port};
            for (int index = 0; index < 2; ++index) {
                auto connection = server.accept_one();
                receive_one_session(connection, receive_dir, state_file);
            }
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    send_one_file(source_a, "first/a.txt", port);
    send_one_file(source_b, "second/b.txt", port);

    server_thread.join();
    if (server_error) {
        std::rethrow_exception(server_error);
    }

    assert(beamdrop::filesystem::read_file(received_a) == content_a);
    assert(beamdrop::filesystem::read_file(received_b) == content_b);

    std::filesystem::remove_all(base_dir);

    std::cout << "persistent_serve_tests passed\n";
    return 0;
}