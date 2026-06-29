#include "beamdrop/filesystem/FileUtils.hpp"
#include "beamdrop/network/TcpClient.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include "beamdrop/transfer/Receiver.hpp"
#include "beamdrop/transfer/Sender.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

using beamdrop::network::TcpClient;
using beamdrop::network::TcpServer;
using beamdrop::transfer::Receiver;
using beamdrop::transfer::Sender;

int main() {
    constexpr std::uint16_t port = 19091;

    const auto base_dir = std::filesystem::temp_directory_path() / "beamdrop_single_file_test";
    const auto send_dir = base_dir / "send";
    const auto receive_dir = base_dir / "received";
    const auto source_path = send_dir / "hello.txt";
    const auto received_path = receive_dir / "nested" / "hello.txt";

    std::filesystem::remove_all(base_dir);
    std::filesystem::create_directories(send_dir);

    const std::vector<std::uint8_t> expected = {'B', 'e', 'a', 'm', 'D', 'r', 'o', 'p', '\n',
                                                'c', 'h', 'u', 'n', 'k', 'e', 'd', '\n'};
    beamdrop::filesystem::write_file(source_path, expected);

    std::exception_ptr server_error;
    std::thread server_thread([&] {
        try {
            TcpServer server{"127.0.0.1", port};
            auto connection = server.accept_one();
            Receiver receiver{connection};
            receiver.receive_one_file(receive_dir);
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    TcpClient client;
    auto connection = client.connect("127.0.0.1", port);
    Sender sender{connection};
    sender.send_file(source_path, "nested/hello.txt");

    server_thread.join();

    if (server_error) {
        std::rethrow_exception(server_error);
    }

    const auto actual = beamdrop::filesystem::read_file(received_path);
    assert(actual == expected);

    std::filesystem::remove_all(base_dir);

    std::cout << "single_file_transfer_tests passed\n";
    return 0;
}
