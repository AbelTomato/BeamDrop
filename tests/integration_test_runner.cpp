#include <iostream>

extern int beamdrop_tcp_hello_tests_main();
extern int beamdrop_single_file_transfer_tests_main();
extern int beamdrop_directory_transfer_tests_main();
extern int beamdrop_resume_transfer_tests_main();
extern int beamdrop_persistent_serve_tests_main();
extern int beamdrop_app_service_single_file_transfer_tests_main();
extern int beamdrop_app_receive_server_service_tests_main();
extern int beamdrop_tcp_server_timeout_tests_main();

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
    failed += run("tcp_hello_tests", beamdrop_tcp_hello_tests_main) != 0;
    failed += run("single_file_transfer_tests", beamdrop_single_file_transfer_tests_main) != 0;
    failed += run("directory_transfer_tests", beamdrop_directory_transfer_tests_main) != 0;
    failed += run("resume_transfer_tests", beamdrop_resume_transfer_tests_main) != 0;
    failed += run("persistent_serve_tests", beamdrop_persistent_serve_tests_main) != 0;
    failed += run("app_service_single_file_transfer_tests",
                  beamdrop_app_service_single_file_transfer_tests_main) != 0;
    failed +=
        run("receive_server_service_tests", beamdrop_app_receive_server_service_tests_main) != 0;
    failed += run("tcp_server_timeout_tests", beamdrop_tcp_server_timeout_tests_main) != 0;
    return failed == 0 ? 0 : 1;
}