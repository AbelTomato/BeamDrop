
#include "beamdrop/app/ReceiveService.hpp"
#include "beamdrop/app/SendService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/filesystem/FileUtils.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include "beamdrop/transfer/Progress.hpp"
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

int main() {
    using namespace std::chrono_literals;

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
    beamdrop::app::TransferProgress last_receive_progress;
    std::vector<beamdrop::app::TransferProgress> receive_progress_events;
    auto receive_result = beamdrop::app::ServiceResult<beamdrop::app::ReceiveResult>::success(
        beamdrop::app::ReceiveResult{});
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
                receive_progress_events.push_back(progress);
                ++receive_progress_count;
            };

            receive_result = beamdrop::app::ReceiveService{}.receive(connection, request, {});
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    beamdrop::app::TransferProgress last_send_progress;
    std::vector<beamdrop::app::TransferProgress> send_progress_events;
    std::size_t send_progress_count = 0;

    beamdrop::app::SendRequest send_request;
    send_request.paths = {source_path};
    send_request.host = "127.0.0.1";
    send_request.port = port;
    send_request.chunk_size = 4;
    send_request.progress_callback = [&](const beamdrop::app::TransferProgress &progress) {
        last_send_progress = progress;
        send_progress_events.push_back(progress);
        ++send_progress_count;
    };

    const auto send_result = beamdrop::app::SendService{}.send(send_request);

    server_thread.join();
    if (server_error) {
        std::rethrow_exception(server_error);
    }

    assert(send_result);
    assert(receive_result);

    const auto actual = beamdrop::filesystem::read_file(received_path);
    assert(actual == expected);

    assert(send_result.value().file_count == 1);
    assert(send_result.value().total_bytes == expected.size());
    assert(receive_result.value().file_count == 1);

    assert(send_progress_count > 0);
    assert(send_progress_events.front().stage == beamdrop::transfer::Stage::TaskStarted);
    assert(send_progress_events.front().file_index == 0);
    assert(send_progress_events.front().file_count == 1);
    assert(send_progress_events[send_progress_events.size() - 2].stage ==
           beamdrop::transfer::Stage::FileCompleted);
    assert(send_progress_events[send_progress_events.size() - 2].relative_path == "hello.txt");
    assert(send_progress_events[send_progress_events.size() - 2].current_file_bytes ==
           expected.size());
    assert(send_progress_events[send_progress_events.size() - 2].current_file_total_bytes ==
           expected.size());
    assert(last_send_progress.direction == beamdrop::app::TransferProgress::Direction::Send);
    assert(last_send_progress.relative_path.empty());
    assert(last_send_progress.current_file_bytes == 0);
    assert(last_send_progress.current_file_total_bytes == 0);
    assert(last_send_progress.file_index == 1);
    assert(last_send_progress.file_count == 1);
    assert(last_send_progress.stage == beamdrop::transfer::Stage::TaskCompleted);

    assert(receive_progress_count > 0);
    assert(receive_progress_events.front().stage == beamdrop::transfer::Stage::TaskStarted);
    assert(receive_progress_events.front().file_index == 0);
    assert(receive_progress_events.front().file_count == 1);
    assert(receive_progress_events[receive_progress_events.size() - 2].stage ==
           beamdrop::transfer::Stage::FileCompleted);
    assert(receive_progress_events[receive_progress_events.size() - 2].relative_path ==
           "hello.txt");
    assert(last_receive_progress.direction == beamdrop::app::TransferProgress::Direction::Receive);
    assert(last_receive_progress.relative_path.empty());
    assert(last_receive_progress.current_file_bytes == 0);
    assert(last_receive_progress.current_file_total_bytes == 0);
    assert(last_receive_progress.file_index == 1);
    assert(last_receive_progress.file_count == 1);
    assert(last_receive_progress.stage == beamdrop::transfer::Stage::TaskCompleted);

    std::filesystem::remove_all(base_dir);

    std::cout << "app_service_single_file_transfer_tests passed\n";
    return 0;
}