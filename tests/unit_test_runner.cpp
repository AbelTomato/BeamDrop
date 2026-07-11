// Keeps existing standalone test sources intact while linking them as one suite.
#include <iostream>

extern int beamdrop_protocol_tests_main();
extern int beamdrop_sha256_tests_main();
extern int beamdrop_file_scanner_tests_main();
extern int beamdrop_logger_tests_main();
extern int beamdrop_config_tests_main();
extern int beamdrop_transfer_manifest_tests_main();
extern int beamdrop_file_info_codec_tests_main();
extern int beamdrop_resume_ack_codec_tests_main();
extern int beamdrop_resume_manager_tests_main();
extern int beamdrop_app_transfer_task_tests_main();
extern int beamdrop_app_send_service_tests_main();
extern int beamdrop_app_receive_service_tests_main();

namespace {

int run(const char *name, int (*test_main)()) {
    std::cout << "[ RUN      ] " << name << '\n';
    const int result = test_main();
    if (result != 0) {
        std::cerr << "[  FAILED  ] " << name << " returned " << result << '\n';
        return result;
    }
    std::cout << "[       OK ] " << name << '\n';
    return 0;
}

} // namespace

int main() {
    int failed = 0;
    failed += run("protocol_tests", beamdrop_protocol_tests_main) != 0;
    failed += run("sha256_tests", beamdrop_sha256_tests_main) != 0;
    failed += run("file_scanner_tests", beamdrop_file_scanner_tests_main) != 0;
    failed += run("logger_tests", beamdrop_logger_tests_main) != 0;
    failed += run("config_tests", beamdrop_config_tests_main) != 0;
    failed += run("transfer_manifest_tests", beamdrop_transfer_manifest_tests_main) != 0;
    failed += run("file_info_codec_tests", beamdrop_file_info_codec_tests_main) != 0;
    failed += run("resume_ack_codec_tests", beamdrop_resume_ack_codec_tests_main) != 0;
    failed += run("resume_manager_tests", beamdrop_resume_manager_tests_main) != 0;
    failed += run("app_transfer_task_tests", beamdrop_app_transfer_task_tests_main) != 0;
    failed += run("app_send_service_tests", beamdrop_app_send_service_tests_main) != 0;
    failed += run("app_receive_service_tests", beamdrop_app_receive_service_tests_main) != 0;
    return failed == 0 ? 0 : 1;
}