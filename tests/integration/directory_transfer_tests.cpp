#include "beamdrop/filesystem/FileScanner.hpp"
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
    constexpr std::uint16_t port = 19092;

    const auto base_dir = std::filesystem::temp_directory_path() / "beamdrop_directory_transfer_test";
    const auto send_dir = base_dir / "send";
    const auto receive_dir = base_dir / "received";

    const auto source_a = send_dir / "a.txt";
    const auto source_b = send_dir / "nested" / "b.txt";
    const auto source_c = send_dir / "nested" / "deep" / "c.bin";

    const auto received_a = receive_dir / "a.txt";
    const auto received_b = receive_dir / "nested" / "b.txt";
    const auto received_c = receive_dir / "nested" / "deep" / "c.bin";

    const std::vector<std::uint8_t> content_a = {'a', '\n'};
    const std::vector<std::uint8_t> content_b = {'B', 'e', 'a', 'm', 'D', 'r', 'o', 'p'};
    const std::vector<std::uint8_t> content_c = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    std::filesystem::remove_all(base_dir);
    beamdrop::filesystem::write_file(source_a, content_a);
    beamdrop::filesystem::write_file(source_b, content_b);
    beamdrop::filesystem::write_file(source_c, content_c);

    const auto entries = beamdrop::filesystem::scan_files(send_dir);
    assert(entries.size() == 3);

    std::exception_ptr server_error;
    std::thread server_thread([&] {
        try {
            TcpServer server{"127.0.0.1", port};
            auto connection = server.accept_one();
            Receiver receiver{connection};
            receiver.receive_files(receive_dir, entries.size());
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    TcpClient client;
    auto connection = client.connect("127.0.0.1", port);
    Sender sender{connection};
    sender.send_files(entries);

    server_thread.join();

    if (server_error) {
        std::rethrow_exception(server_error);
    }

    assert(beamdrop::filesystem::read_file(received_a) == content_a);
    assert(beamdrop::filesystem::read_file(received_b) == content_b);
    assert(beamdrop::filesystem::read_file(received_c) == content_c);

    std::filesystem::remove_all(base_dir);

    std::cout << "directory_transfer_tests passed\n";
    return 0;
}