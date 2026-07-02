#include "beamdrop/filesystem/FileUtils.hpp"
#include "beamdrop/network/TcpClient.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include "beamdrop/transfer/Receiver.hpp"
#include "beamdrop/transfer/ResumeManager.hpp"
#include "beamdrop/transfer/Sender.hpp"
#include "beamdrop/utils/Sha256.hpp"

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
using beamdrop::transfer::ResumeManager;
using beamdrop::transfer::Sender;

int main() {
    constexpr std::uint16_t port = 19093;

    const auto base_dir = std::filesystem::temp_directory_path() / "beamdrop_resume_transfer_test";
    const auto send_dir = base_dir / "send";
    const auto receive_dir = base_dir / "received";
    const auto state_file = base_dir / "transfer_state.json";
    const auto source_path = send_dir / "large.txt";
    const auto received_path = receive_dir / "nested" / "large.txt";
    const std::string relative_path = "nested/large.txt";

    std::filesystem::remove_all(base_dir);
    std::filesystem::create_directories(send_dir);

    const std::vector<std::uint8_t> expected = {
        'B', 'e', 'a', 'm', 'D', 'r', 'o', 'p', '\n',
        'r', 'e', 's', 'u', 'm', 'e', '\n',
        'p', 'a', 'y', 'l', 'o', 'a', 'd', '\n'};
    const std::vector<std::uint8_t> existing_prefix{expected.begin(), expected.begin() + 12};

    beamdrop::filesystem::write_file(source_path, expected);
    beamdrop::filesystem::write_file(received_path, existing_prefix);

    const auto source_hash = beamdrop::utils::sha256_file(source_path, 4);
    ResumeManager{state_file}.update_offset(relative_path,
                                            expected.size(),
                                            source_hash,
                                            existing_prefix.size());

    std::exception_ptr server_error;
    std::thread server_thread([&] {
        try {
            TcpServer server{"127.0.0.1", port};
            auto connection = server.accept_one();
            Receiver receiver{connection, {}, true, state_file};
            receiver.receive_one_file(receive_dir);
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    TcpClient client;
    auto connection = client.connect("127.0.0.1", port);
    Sender sender{connection, {}, 4};
    sender.send_file(source_path, relative_path);

    server_thread.join();
    if (server_error) {
        std::rethrow_exception(server_error);
    }

    const auto actual = beamdrop::filesystem::read_file(received_path);
    assert(actual == expected);
    assert(beamdrop::utils::sha256_file(received_path, 4) == source_hash);
    assert(ResumeManager{state_file}.load().empty());

    std::filesystem::remove_all(base_dir);

    std::cout << "resume_transfer_tests passed\n";
    return 0;
}