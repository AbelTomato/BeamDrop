#include "beamdrop/filesystem/FileScanner.hpp"
#include "beamdrop/filesystem/FileUtils.hpp"
#include "beamdrop/network/TcpClient.hpp"
#include "beamdrop/network/TcpServer.hpp"
#include "beamdrop/protocol/Packet.hpp"
#include "beamdrop/protocol/PacketIO.hpp"
#include "beamdrop/protocol/PacketType.hpp"
#include "beamdrop/transfer/Progress.hpp"
#include "beamdrop/transfer/Receiver.hpp"
#include "beamdrop/transfer/ResumeManager.hpp"
#include "beamdrop/transfer/Sender.hpp"
#include "beamdrop/transfer/TransferManifest.hpp"

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
using beamdrop::protocol::Packet;
using beamdrop::protocol::PacketType;
using beamdrop::transfer::ProgressDirection;
using beamdrop::transfer::ProgressEvent;
using beamdrop::transfer::Receiver;
using beamdrop::transfer::ResumeManager;
using beamdrop::transfer::Sender;
using beamdrop::transfer::TransferManifest;
using beamdrop::transfer::TransferManifestCodec;

int main() {
    using namespace std::chrono_literals;

    constexpr std::uint16_t port = 19092;

    const auto base_dir =
        std::filesystem::temp_directory_path() / "beamdrop_directory_transfer_test";
    const auto send_dir = base_dir / "send";
    const auto receive_dir = base_dir / "received";
    const auto state_file = base_dir / "transfer_state.json";

    const auto source_a = send_dir / "a.txt";
    const auto source_b = send_dir / "nested" / "b.txt";
    const auto source_c = send_dir / "nested" / "deep" / "c.bin";
    const auto empty_dir = send_dir / "empty";

    const auto received_a = receive_dir / "send" / "a.txt";
    const auto received_b = receive_dir / "send" / "nested" / "b.txt";
    const auto received_c = receive_dir / "send" / "nested" / "deep" / "c.bin";
    const auto received_empty_dir = receive_dir / "send" / "empty";

    const std::vector<std::uint8_t> content_a = {'a', '\n'};
    const std::vector<std::uint8_t> content_b = {'B', 'e', 'a', 'm', 'D', 'r', 'o', 'p'};
    const std::vector<std::uint8_t> content_c = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    std::filesystem::remove_all(base_dir);
    beamdrop::filesystem::write_file(source_a, content_a);
    beamdrop::filesystem::write_file(source_b, content_b);
    beamdrop::filesystem::write_file(source_c, content_c);
    std::filesystem::create_directories(empty_dir);

    const auto entries = beamdrop::filesystem::scan_files(send_dir);
    const auto directories = beamdrop::filesystem::scan_directories(send_dir);
    assert(entries.size() == 3);
    std::uint64_t total_bytes = 0;
    for (const auto &entry : entries) {
        total_bytes += entry.size;
    }

    std::exception_ptr server_error;
    TransferManifest received_manifest;
    std::vector<ProgressEvent> receive_file_completed;
    std::vector<ProgressEvent> receive_task_completed;
    std::thread server_thread([&] {
        try {
            TcpServer server{"127.0.0.1", port};
            auto connection = server.accept_one();

            const auto hello = beamdrop::protocol::read_packet(connection);
            assert(hello.header.type == PacketType::Hello);
            received_manifest = TransferManifestCodec::decode(hello.payload);
            assert(received_manifest.file_count == entries.size());
            assert(received_manifest.directory_count == directories.size());
            assert(received_manifest.total_bytes == total_bytes);

            Receiver receiver{connection,
                              [&](const ProgressEvent &event) {
                                  if (event.stage == beamdrop::transfer::Stage::FileCompleted) {
                                      receive_file_completed.push_back(event);
                                  }
                                  if (event.stage == beamdrop::transfer::Stage::TaskCompleted) {
                                      receive_task_completed.push_back(event);
                                  }
                              },
                              true, state_file};
            receiver.receive_task(receive_dir,
                                  static_cast<std::size_t>(received_manifest.file_count),
                                  static_cast<std::size_t>(received_manifest.directory_count));
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    TcpClient client;
    auto connection = client.connect("127.0.0.1", port);
    Packet hello;
    hello.header.type = PacketType::Hello;
    hello.payload = TransferManifestCodec::encode(
        TransferManifest{static_cast<std::uint64_t>(entries.size()), total_bytes,
                         static_cast<std::uint64_t>(directories.size())});
    beamdrop::protocol::write_packet(connection, hello);

    std::vector<ProgressEvent> send_file_completed;
    std::vector<ProgressEvent> send_task_completed;
    Sender sender{connection,
                  [&](const ProgressEvent &event) {
                      if (event.stage == beamdrop::transfer::Stage::FileCompleted) {
                          send_file_completed.push_back(event);
                      }
                      if (event.stage == beamdrop::transfer::Stage::TaskCompleted) {
                          send_task_completed.push_back(event);
                      }
                  },
                  4};
    sender.send_task(entries, directories);

    server_thread.join();

    if (server_error) {
        std::rethrow_exception(server_error);
    }

    assert(beamdrop::filesystem::read_file(received_a) == content_a);
    assert(beamdrop::filesystem::read_file(received_b) == content_b);
    assert(beamdrop::filesystem::read_file(received_c) == content_c);
    assert(std::filesystem::is_directory(received_empty_dir));

    assert(send_file_completed.size() == entries.size());
    assert(receive_file_completed.size() == entries.size());
    assert(send_task_completed.size() == 1);
    assert(receive_task_completed.size() == 1);
    assert(send_task_completed.front().file_index == entries.size());
    assert(send_task_completed.front().file_count == entries.size());
    assert(receive_task_completed.front().file_index == entries.size());
    assert(receive_task_completed.front().file_count == entries.size());
    for (std::size_t index = 0; index < entries.size(); ++index) {
        assert(send_file_completed[index].direction == ProgressDirection::Send);
        assert(send_file_completed[index].relative_path == entries[index].relative_path);
        assert(send_file_completed[index].current_file_bytes == entries[index].size);
        assert(send_file_completed[index].current_file_total_bytes == entries[index].size);
        assert(send_file_completed[index].file_index == index + 1);
        assert(send_file_completed[index].file_count == entries.size());

        assert(receive_file_completed[index].direction == ProgressDirection::Receive);
        assert(receive_file_completed[index].relative_path == entries[index].relative_path);
        assert(receive_file_completed[index].current_file_bytes == entries[index].size);
        assert(receive_file_completed[index].current_file_total_bytes == entries[index].size);
        assert(receive_file_completed[index].file_index == index + 1);
        assert(receive_file_completed[index].file_count == entries.size());
    }
    assert(ResumeManager{state_file}.load().empty());

    std::filesystem::remove_all(base_dir);

    std::cout << "directory_transfer_tests passed\n";
    return 0;
}