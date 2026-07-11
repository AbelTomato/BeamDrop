
#include "beamdrop/app/SendService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/filesystem/FileUtils.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include "beamdrop/app/ReceiveService.hpp"
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <thread>
int main() {
    constexpr std::uint16_t port = 19101;

    const auto base_dir =
        std::filesystem::temp_directory_path() / "beamdrop_app_service_single_file_test";
    const auto send_dir = base_dir / "send";
    const auto receive_dir = base_dir / "received";
    const auto state_file = base_dir / "transfer_state.json";
    const auto source_path = send_dir / "hello.txt";
    const auto received_path = receive_dir / "hello.txt";

    std::filesystem::remove_all(base_dir);
    std::filesystem::create_directories(send_dir);

    const std::vector<std::uint8_t> expected = {'B', 'e', 'a', 'm', 'D', 'r', 'o', 'p', '\n'};
    beamdrop::filesystem::write_file(source_path, expected);

    std::exception_ptr server_error;
    beamdrop::app::ReceiveResult receive_result;
    beamdrop::app::TransferProgress last_receive_progress;
    std::size_t receive_progress_count = 0;

    std::thread server_thread([&] {
        try {
            beamdrop::network::TcpServer server("127.0.0.1", port);
            auto connection = server.accept_one();

            beamdrop::app::ReceiveRequest request;
            request.save_dir = receive_dir;
            request.state_file = state_file;
            request.enable_resume = false;
            request.progress_callback = [&](const beamdrop::app::TransferProgress &progress) {
                last_receive_progress = progress;
                ++receive_progress_count;
            };

            receive_result = beamdrop::app::ReceiveService{}.receive(connection, request);
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    beamdrop::app::TransferProgress last_send_progress;
    std::size_t send_progress_count = 0;

    beamdrop::app::SendRequest send_request;
    send_request.paths = {source_path};
    send_request.host = "127.0.0.1";
    send_request.port = port;
    send_request.chunk_size = 4;
    send_request.progress_callback = [&](const beamdrop::app::TransferProgress &progress) {
        last_send_progress = progress;
        ++send_progress_count;
    };

    const auto send_result = beamdrop::app::SendService{}.send(send_request);

    server_thread.join();
    if (server_error) {
        std::rethrow_exception(server_error);
    }

    const auto actual = beamdrop::filesystem::read_file(received_path);
    assert(actual == expected);

    assert(send_result.file_count == 1);
    assert(send_result.total_bytes == expected.size());
    assert(receive_result.file_count == 1);

    assert(send_progress_count > 0);
    assert(last_send_progress.direction == beamdrop::app::TransferProgress::Direction::Send);
    assert(last_send_progress.relative_path == "hello.txt");
    assert(last_send_progress.current_file_bytes == expected.size());
    assert(last_send_progress.current_file_total_bytes == expected.size());
    assert(last_send_progress.file_index == 1);
    assert(last_send_progress.file_count == 1);
    assert(last_send_progress.file_complete);

    assert(receive_progress_count > 0);
    assert(last_receive_progress.direction == beamdrop::app::TransferProgress::Direction::Receive);
    assert(last_receive_progress.relative_path == "hello.txt");
    assert(last_receive_progress.current_file_bytes == expected.size());
    assert(last_receive_progress.current_file_total_bytes == expected.size());
    assert(last_receive_progress.file_index == 1);
    assert(last_receive_progress.file_count == 1);
    assert(last_receive_progress.file_complete);

    std::filesystem::remove_all(base_dir);

    std::cout << "app_service_single_file_transfer_tests passed\n";
    return 0;
}