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

option(BEAMDROP_BUILD_CLI "Build the BeamDrop CLI executable" ON)
option(BEAMDROP_BUILD_TESTS "Build BeamDrop CTest suites" ON)
option(BEAMDROP_BUILD_PYTHON "Build Python native bindings" OFF)

find_package(Threads REQUIRED)

add_library(beamdrop_project_warnings INTERFACE)

if (MSVC)
    target_compile_options(beamdrop_project_warnings INTERFACE /W4)
else()
    target_compile_options(beamdrop_project_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

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
    src/transfer/DirectoryInfoCodec.cpp
    src/transfer/Receiver.cpp
    src/transfer/ResumeAckCodec.cpp
    src/transfer/ResumeManager.cpp
    src/transfer/Sender.cpp
    src/transfer/TransferManifest.cpp
    src/utils/Sha256.cpp
    src/app/SendService.cpp
    src/app/ReceiveService.cpp
    src/app/ReceiveServerService.cpp)

add_library(beamdrop_core STATIC
    ${BEAMDROP_CORE_SOURCES}
)

target_include_directories(beamdrop_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

set_target_properties(beamdrop_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(beamdrop_core
    PUBLIC
        Threads::Threads
    PRIVATE
        beamdrop_project_warnings
)

if (WIN32)
    target_link_libraries(beamdrop_core PUBLIC ws2_32)
endif()

if (BEAMDROP_BUILD_CLI)
    add_executable(beamdrop
        src/main.cpp
    )

    target_link_libraries(beamdrop PRIVATE beamdrop_core beamdrop_project_warnings)
endif()

if (BEAMDROP_BUILD_TESTS)
    enable_testing()

    function(add_beamdrop_test_object target source entrypoint)
        add_library(${target} OBJECT ${source})
        target_compile_definitions(${target} PRIVATE main=${entrypoint} ${ARGN})
        target_link_libraries(${target} PRIVATE beamdrop_core beamdrop_project_warnings)
    endfunction()

    add_beamdrop_test_object(protocol_tests_obj tests/unit/protocol_tests.cpp beamdrop_protocol_tests_main)
    add_beamdrop_test_object(sha256_tests_obj tests/unit/sha256_tests.cpp beamdrop_sha256_tests_main)
    add_beamdrop_test_object(file_scanner_tests_obj tests/unit/file_scanner_tests.cpp beamdrop_file_scanner_tests_main)
    add_beamdrop_test_object(logger_tests_obj tests/unit/logger_tests.cpp beamdrop_logger_tests_main)
    add_beamdrop_test_object(config_tests_obj tests/unit/config_tests.cpp beamdrop_config_tests_main)
    add_beamdrop_test_object(transfer_manifest_tests_obj tests/unit/transfer_manifest_tests.cpp beamdrop_transfer_manifest_tests_main)
    add_beamdrop_test_object(file_info_codec_tests_obj tests/unit/file_info_codec_tests.cpp beamdrop_file_info_codec_tests_main)
    add_beamdrop_test_object(resume_ack_codec_tests_obj tests/unit/resume_ack_codec_tests.cpp beamdrop_resume_ack_codec_tests_main)
    add_beamdrop_test_object(resume_manager_tests_obj tests/unit/resume_manager_tests.cpp beamdrop_resume_manager_tests_main)
    add_beamdrop_test_object(app_transfer_task_tests_obj tests/unit/app_transfer_task_tests.cpp beamdrop_app_transfer_task_tests_main)
    add_beamdrop_test_object(app_send_service_tests_obj tests/unit/app_send_service_tests.cpp beamdrop_app_send_service_tests_main test_throw_if_cancelled=beamdrop_app_send_test_throw_if_cancelled)
    add_beamdrop_test_object(app_receive_service_tests_obj tests/unit/app_receive_service_tests.cpp beamdrop_app_receive_service_tests_main test_throw_if_cancelled=beamdrop_app_receive_test_throw_if_cancelled)

    add_executable(beamdrop_unit_tests
        tests/unit_test_runner.cpp
        $<TARGET_OBJECTS:protocol_tests_obj>
        $<TARGET_OBJECTS:sha256_tests_obj>
        $<TARGET_OBJECTS:file_scanner_tests_obj>
        $<TARGET_OBJECTS:logger_tests_obj>
        $<TARGET_OBJECTS:config_tests_obj>
        $<TARGET_OBJECTS:transfer_manifest_tests_obj>
        $<TARGET_OBJECTS:file_info_codec_tests_obj>
        $<TARGET_OBJECTS:resume_ack_codec_tests_obj>
        $<TARGET_OBJECTS:resume_manager_tests_obj>
        $<TARGET_OBJECTS:app_transfer_task_tests_obj>
        $<TARGET_OBJECTS:app_send_service_tests_obj>
        $<TARGET_OBJECTS:app_receive_service_tests_obj>
    )
    target_link_libraries(beamdrop_unit_tests PRIVATE beamdrop_core beamdrop_project_warnings)
    add_test(NAME beamdrop_unit_tests COMMAND beamdrop_unit_tests)

    add_beamdrop_test_object(tcp_hello_tests_obj tests/integration/tcp_hello_tests.cpp beamdrop_tcp_hello_tests_main)
    add_beamdrop_test_object(single_file_transfer_tests_obj tests/integration/single_file_transfer_tests.cpp beamdrop_single_file_transfer_tests_main)
    add_beamdrop_test_object(directory_transfer_tests_obj tests/integration/directory_transfer_tests.cpp beamdrop_directory_transfer_tests_main)
    add_beamdrop_test_object(resume_transfer_tests_obj tests/integration/resume_transfer_tests.cpp beamdrop_resume_transfer_tests_main)
    add_beamdrop_test_object(persistent_serve_tests_obj tests/integration/persistent_serve_tests.cpp beamdrop_persistent_serve_tests_main)
    add_beamdrop_test_object(app_service_single_file_transfer_tests_obj tests/integration/app_service_single_file_transfer_tests.cpp beamdrop_app_service_single_file_transfer_tests_main)
    add_beamdrop_test_object(app_receive_server_service_tests_obj tests/integration/app_receive_server_service_tests.cpp beamdrop_app_receive_server_service_tests_main)
    add_beamdrop_test_object(tcp_server_timeout_tests_obj tests/integration/tcp_server_timeout_tests.cpp beamdrop_tcp_server_timeout_tests_main)

    add_executable(beamdrop_integration_tests
        tests/integration_test_runner.cpp
        $<TARGET_OBJECTS:tcp_hello_tests_obj>
        $<TARGET_OBJECTS:single_file_transfer_tests_obj>
        $<TARGET_OBJECTS:directory_transfer_tests_obj>
        $<TARGET_OBJECTS:resume_transfer_tests_obj>
        $<TARGET_OBJECTS:persistent_serve_tests_obj>
        $<TARGET_OBJECTS:app_service_single_file_transfer_tests_obj>
        $<TARGET_OBJECTS:app_receive_server_service_tests_obj>
        $<TARGET_OBJECTS:tcp_server_timeout_tests_obj>
    )
    target_link_libraries(beamdrop_integration_tests PRIVATE beamdrop_core beamdrop_project_warnings)
    add_test(NAME beamdrop_integration_tests COMMAND beamdrop_integration_tests)

    if (WIN32)
        add_test(
            NAME beamdrop_cli_serve_smoke
            COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/cli_serve_smoke.ps1
                    -BeamDropExe $<TARGET_FILE:beamdrop>
        )
    endif()
endif()

if (BEAMDROP_BUILD_PYTHON)
    add_subdirectory(bindings/python)
endif()