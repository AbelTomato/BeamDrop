cmake_minimum_required(VERSION 3.20)

project(BeamDrop
    VERSION 0.1.0
    DESCRIPTION "BeamDrop / 邻光 - LAN file transfer CLI"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(beamdrop
    src/main.cpp
    src/config/AppConfig.cpp
    src/filesystem/FileScanner.cpp
    src/filesystem/FileUtils.cpp
    src/logger/Logger.cpp
    src/network/TcpClient.cpp
    src/network/TcpConnection.cpp
    src/network/TcpServer.cpp
    src/protocol/PacketIO.cpp
    src/protocol/PacketParser.cpp
    src/protocol/Serializer.cpp
    src/transfer/FileInfoCodec.cpp
    src/transfer/Receiver.cpp
    src/transfer/ResumeAckCodec.cpp
    src/transfer/ResumeManager.cpp
    src/transfer/Sender.cpp
    src/transfer/TransferManifest.cpp
    src/utils/Sha256.cpp
)

target_include_directories(beamdrop
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(beamdrop PRIVATE /W4)
else()
    target_compile_options(beamdrop PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    target_link_libraries(beamdrop PRIVATE ws2_32)
endif()

enable_testing()

add_executable(protocol_tests
    tests/unit/protocol_tests.cpp
    src/protocol/PacketParser.cpp
    src/protocol/Serializer.cpp
)

target_include_directories(protocol_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(protocol_tests PRIVATE /W4)
else()
    target_compile_options(protocol_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME protocol_tests COMMAND protocol_tests)

add_executable(sha256_tests
    tests/unit/sha256_tests.cpp
    src/utils/Sha256.cpp
)

target_include_directories(sha256_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(sha256_tests PRIVATE /W4)
else()
    target_compile_options(sha256_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME sha256_tests COMMAND sha256_tests)

add_executable(file_scanner_tests
    tests/unit/file_scanner_tests.cpp
    src/filesystem/FileScanner.cpp
    src/filesystem/FileUtils.cpp
)

target_include_directories(file_scanner_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(file_scanner_tests PRIVATE /W4)
else()
    target_compile_options(file_scanner_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME file_scanner_tests COMMAND file_scanner_tests)

add_executable(logger_tests
    tests/unit/logger_tests.cpp
    src/logger/Logger.cpp
)

target_include_directories(logger_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(logger_tests PRIVATE /W4)
else()
    target_compile_options(logger_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME logger_tests COMMAND logger_tests)

add_executable(config_tests
    tests/unit/config_tests.cpp
    src/config/AppConfig.cpp
)

target_include_directories(config_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(config_tests PRIVATE /W4)
else()
    target_compile_options(config_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME config_tests COMMAND config_tests)

add_executable(transfer_manifest_tests
    tests/unit/transfer_manifest_tests.cpp
    src/transfer/TransferManifest.cpp
)

target_include_directories(transfer_manifest_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(transfer_manifest_tests PRIVATE /W4)
else()
    target_compile_options(transfer_manifest_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME transfer_manifest_tests COMMAND transfer_manifest_tests)

add_executable(file_info_codec_tests
    tests/unit/file_info_codec_tests.cpp
    src/transfer/FileInfoCodec.cpp
)

target_include_directories(file_info_codec_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(file_info_codec_tests PRIVATE /W4)
else()
    target_compile_options(file_info_codec_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME file_info_codec_tests COMMAND file_info_codec_tests)

add_executable(resume_ack_codec_tests
    tests/unit/resume_ack_codec_tests.cpp
    src/transfer/ResumeAckCodec.cpp
)

target_include_directories(resume_ack_codec_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(resume_ack_codec_tests PRIVATE /W4)
else()
    target_compile_options(resume_ack_codec_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME resume_ack_codec_tests COMMAND resume_ack_codec_tests)

add_executable(resume_manager_tests
    tests/unit/resume_manager_tests.cpp
    src/transfer/ResumeManager.cpp
)

target_include_directories(resume_manager_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(resume_manager_tests PRIVATE /W4)
else()
    target_compile_options(resume_manager_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME resume_manager_tests COMMAND resume_manager_tests)

add_executable(tcp_hello_tests
    tests/integration/tcp_hello_tests.cpp
    src/network/TcpClient.cpp
    src/network/TcpConnection.cpp
    src/network/TcpServer.cpp
    src/protocol/PacketIO.cpp
    src/protocol/PacketParser.cpp
    src/protocol/Serializer.cpp
)

target_include_directories(tcp_hello_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(tcp_hello_tests PRIVATE /W4)
else()
    target_compile_options(tcp_hello_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    target_link_libraries(tcp_hello_tests PRIVATE ws2_32)
endif()

add_test(NAME tcp_hello_tests COMMAND tcp_hello_tests)

add_executable(single_file_transfer_tests
    tests/integration/single_file_transfer_tests.cpp
    src/filesystem/FileScanner.cpp
    src/filesystem/FileUtils.cpp
    src/network/TcpClient.cpp
    src/network/TcpConnection.cpp
    src/network/TcpServer.cpp
    src/protocol/PacketIO.cpp
    src/protocol/PacketParser.cpp
    src/protocol/Serializer.cpp
    src/transfer/FileInfoCodec.cpp
    src/transfer/Receiver.cpp
    src/transfer/ResumeAckCodec.cpp
    src/transfer/ResumeManager.cpp
    src/transfer/Sender.cpp
    src/transfer/TransferManifest.cpp
    src/utils/Sha256.cpp
)

target_include_directories(single_file_transfer_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(single_file_transfer_tests PRIVATE /W4)
else()
    target_compile_options(single_file_transfer_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    target_link_libraries(single_file_transfer_tests PRIVATE ws2_32)
endif()

add_test(NAME single_file_transfer_tests COMMAND single_file_transfer_tests)

add_executable(directory_transfer_tests
    tests/integration/directory_transfer_tests.cpp
    src/filesystem/FileScanner.cpp
    src/filesystem/FileUtils.cpp
    src/network/TcpClient.cpp
    src/network/TcpConnection.cpp
    src/network/TcpServer.cpp
    src/protocol/PacketIO.cpp
    src/protocol/PacketParser.cpp
    src/protocol/Serializer.cpp
    src/transfer/FileInfoCodec.cpp
    src/transfer/Receiver.cpp
    src/transfer/ResumeAckCodec.cpp
    src/transfer/ResumeManager.cpp
    src/transfer/Sender.cpp
    src/transfer/TransferManifest.cpp
    src/utils/Sha256.cpp
)

target_include_directories(directory_transfer_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(directory_transfer_tests PRIVATE /W4)
else()
    target_compile_options(directory_transfer_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    target_link_libraries(directory_transfer_tests PRIVATE ws2_32)
endif()

add_test(NAME directory_transfer_tests COMMAND directory_transfer_tests)

add_executable(resume_transfer_tests
    tests/integration/resume_transfer_tests.cpp
    src/filesystem/FileScanner.cpp
    src/filesystem/FileUtils.cpp
    src/network/TcpClient.cpp
    src/network/TcpConnection.cpp
    src/network/TcpServer.cpp
    src/protocol/PacketIO.cpp
    src/protocol/PacketParser.cpp
    src/protocol/Serializer.cpp
    src/transfer/FileInfoCodec.cpp
    src/transfer/Receiver.cpp
    src/transfer/ResumeAckCodec.cpp
    src/transfer/ResumeManager.cpp
    src/transfer/Sender.cpp
    src/transfer/TransferManifest.cpp
    src/utils/Sha256.cpp
)

target_include_directories(resume_transfer_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(resume_transfer_tests PRIVATE /W4)
else()
    target_compile_options(resume_transfer_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    target_link_libraries(resume_transfer_tests PRIVATE ws2_32)
endif()

add_test(NAME resume_transfer_tests COMMAND resume_transfer_tests)

add_executable(persistent_serve_tests
    tests/integration/persistent_serve_tests.cpp
    src/filesystem/FileScanner.cpp
    src/filesystem/FileUtils.cpp
    src/network/TcpClient.cpp
    src/network/TcpConnection.cpp
    src/network/TcpServer.cpp
    src/protocol/PacketIO.cpp
    src/protocol/PacketParser.cpp
    src/protocol/Serializer.cpp
    src/transfer/FileInfoCodec.cpp
    src/transfer/Receiver.cpp
    src/transfer/ResumeAckCodec.cpp
    src/transfer/ResumeManager.cpp
    src/transfer/Sender.cpp
    src/transfer/TransferManifest.cpp
    src/utils/Sha256.cpp
)

target_include_directories(persistent_serve_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(persistent_serve_tests PRIVATE /W4)
else()
    target_compile_options(persistent_serve_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    target_link_libraries(persistent_serve_tests PRIVATE ws2_32)
endif()

add_test(NAME persistent_serve_tests COMMAND persistent_serve_tests)
