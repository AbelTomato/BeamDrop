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
set(BEAMDROP_CORE_SOURCES
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
    src/app/SendService.cpp
    src/app/ReceiveService.cpp)

add_library(beamdrop_core STATIC
    ${BEAMDROP_CORE_SOURCES}
)

target_include_directories(beamdrop_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(beamdrop_core PRIVATE /W4)
else()
    target_compile_options(beamdrop_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    target_link_libraries(beamdrop_core PUBLIC ws2_32)
endif()

add_executable(beamdrop
    src/main.cpp
)

target_link_libraries(beamdrop PRIVATE beamdrop_core)

enable_testing()

function(add_beamdrop_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE beamdrop_core)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_beamdrop_test(protocol_tests tests/unit/protocol_tests.cpp)

add_beamdrop_test(sha256_tests tests/unit/sha256_tests.cpp)

add_beamdrop_test(file_scanner_tests tests/unit/file_scanner_tests.cpp)

add_beamdrop_test(logger_tests tests/unit/logger_tests.cpp)

add_beamdrop_test(config_tests tests/unit/config_tests.cpp)

add_beamdrop_test(transfer_manifest_tests tests/unit/transfer_manifest_tests.cpp)

add_beamdrop_test(file_info_codec_tests tests/unit/file_info_codec_tests.cpp)

add_beamdrop_test(resume_ack_codec_tests tests/unit/resume_ack_codec_tests.cpp)

add_beamdrop_test(resume_manager_tests tests/unit/resume_manager_tests.cpp)

add_beamdrop_test(tcp_hello_tests tests/integration/tcp_hello_tests.cpp)

add_beamdrop_test(single_file_transfer_tests tests/integration/single_file_transfer_tests.cpp)

add_beamdrop_test(directory_transfer_tests tests/integration/directory_transfer_tests.cpp)

add_beamdrop_test(resume_transfer_tests tests/integration/resume_transfer_tests.cpp)

add_beamdrop_test(persistent_serve_tests tests/integration/persistent_serve_tests.cpp)

add_beamdrop_test(app_transfer_task_tests tests/unit/app_transfer_task_tests.cpp)

add_beamdrop_test(app_send_service_tests tests/unit/app_send_service_tests.cpp)

add_beamdrop_test(app_receive_service_tests tests/unit/app_receive_service_tests.cpp)

add_beamdrop_test(app_service_single_file_transfer_tests tests/integration/app_service_single_file_transfer_tests.cpp)