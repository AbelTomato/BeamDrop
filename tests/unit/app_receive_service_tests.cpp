#include "beamdrop/app/ReceiveService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/network/TcpConnection.hpp"

#include <cassert>
#include <filesystem>
#include <stop_token>

void expect_service_error(const beamdrop::app::ReceiveRequest &request,
                          beamdrop::app::ServiceError::Code expect_code
                          ) {
    try {
        (void)beamdrop::app::ReceiveService{}.receive(beamdrop::network::TcpConnection{}, request);
        assert(false && "expected ServiceException, but no exception was thrown!");
    } catch (const beamdrop::app::ServiceException &error) {
        assert(error.code() == expect_code);
    }
}

void test_receive_request_error() {
    auto request =
        beamdrop::app::ReceiveRequest{{}, {}, true, "./transfer_state.json", "./data"};
    beamdrop::app::ServiceError::Code expect_code =
        beamdrop::app::ServiceError::Code::InvalidRequest;

    request.state_file = std::filesystem::path{""};
    expect_service_error(request, expect_code);

    request.state_file = std::filesystem::path{"./transfer_state.json"};
    request.save_dir = std::filesystem::path{""};
    expect_service_error(request, expect_code);
}

void test_throw_if_cancelled() {
    std::stop_source source;
    std::stop_token token = source.get_token();
    auto request = beamdrop::app::ReceiveRequest{
        {}, token, true, "./transfer_state.json", "./data"};

    source.request_stop();
    expect_service_error(request, beamdrop::app::ServiceError::Code::Cancelled);
}

int main() {
    test_receive_request_error();
    test_throw_if_cancelled();

    return 0;
}