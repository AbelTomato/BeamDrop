#include "beamdrop/app/ReceiveService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include "beamdrop/network/TcpConnection.hpp"

#include <cassert>
#include <filesystem>
#include <stop_token>

void expect_service_error(const beamdrop::app::ReceiveRequest &request,
                          beamdrop::app::ErrorCode expect_code, std::stop_token stop_token = {}) {
    beamdrop::app::ServiceResult<beamdrop::app::ReceiveResult> result =
        beamdrop::app::ReceiveService{}.receive(beamdrop::network::TcpConnection{}, request,
                                                stop_token);
    assert(!result);
    assert(result.error().code == expect_code);
}

void test_receive_request_error() {
    auto request = beamdrop::app::ReceiveRequest{{}, true, "./transfer_state.json", "./data"};
    beamdrop::app::ErrorCode expect_code = beamdrop::app::ErrorCode::InvalidRequest;

    request.state_file = std::filesystem::path{""};
    expect_service_error(request, expect_code);

    request.state_file = std::filesystem::path{"./transfer_state.json"};
    request.save_dir = std::filesystem::path{""};
    expect_service_error(request, expect_code);
}

void test_throw_if_cancelled() {
    std::stop_source source;
    std::stop_token token = source.get_token();
    auto request = beamdrop::app::ReceiveRequest{{}, true, "./transfer_state.json", "./data"};

    source.request_stop();
    expect_service_error(request, beamdrop::app::ErrorCode::Cancelled, token);
}

int main() {
    test_receive_request_error();
    test_throw_if_cancelled();

    return 0;
}