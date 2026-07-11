#include "beamdrop/app/SendService.hpp"
#include "beamdrop/app/TransferTask.hpp"

#include <cassert>
#include <filesystem>
#include <stop_token>

void expect_service_error(const beamdrop::app::SendRequest &request,
                          beamdrop::app::ServiceError::Code expect_code) {
    try {
        (void)beamdrop::app::SendService{}.send(request);
        assert(false && "expected ServiceException, but no exception was thrown!");
    } catch (const beamdrop::app::ServiceException &error) {
        assert(error.code() == expect_code);
    }
}

void test_send_request_error() {
    auto request = beamdrop::app::SendRequest{std::vector<std::filesystem::path>{}, "0.0.0.0", 9090};
    beamdrop::app::ServiceError::Code expect_code = beamdrop::app::ServiceError::Code::InvalidRequest;
    expect_service_error(request, expect_code);

    request.paths.push_back(std::filesystem::path{""});
    expect_service_error(request, expect_code);

    request.paths[0] = static_cast<std::filesystem::path>("./data.json");
    request.host = "";
    expect_service_error(request, expect_code);

    request.host = "0.0.0.0";
    request.port = 0;
    expect_service_error(request, expect_code);

    request.port = 9090;
    request.chunk_size = 0;
    expect_service_error(request, expect_code);
}

void test_throw_if_cancelled() {
     std::stop_source source;
     std::stop_token token = source.get_token();
     
    auto request = beamdrop::app::SendRequest{std::vector<std::filesystem::path>{"./data.json"},
                                              "0.0.0.0", 9090, 1024 * 1024, {}, token};

    source.request_stop();
    expect_service_error(request, beamdrop::app::ServiceError::Code::Cancelled);
}

int main() {
    test_send_request_error();
    test_throw_if_cancelled();

    return 0;
}