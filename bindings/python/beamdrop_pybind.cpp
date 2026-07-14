#include "beamdrop/app/ReceiveServerService.hpp"
#include "beamdrop/app/ReceiveService.hpp"
#include "beamdrop/app/SendService.hpp"
#include "beamdrop/app/TransferTask.hpp"
#include "pybind11/cast.h"
#include "pybind11/detail/common.h"
#include "pybind11/gil.h"
#include "pybind11/pytypes.h"
#include "pyerrors.h"
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;

namespace {
[[noreturn]] void raise_service_error(const py::object &exception_type,
                                      const beamdrop::app::ServiceError &error) {
    auto exception = exception_type(py::str(error.message));
    exception.attr("code") = py::cast(error.code);
    PyErr_SetObject(exception_type.ptr(), exception.ptr());
    throw py::error_already_set();
}

class PythonProgressCallbackError : public std::runtime_error {
  public:
    PythonProgressCallbackError(const std::string &message) : std::runtime_error{message} {}
};
} // namespace

PYBIND11_MODULE(beamdrop_native, m) {
    m.doc() = "BeamDrop C++ application-service binding";

    py::enum_<beamdrop::app::ErrorCode>(m, "ErrorCode")
        .value("INVALID_REQUEST", beamdrop::app::ErrorCode::InvalidRequest)
        .value("NETWORK_FAILURE", beamdrop::app::ErrorCode::NetworkFailure)
        .value("PROTOCOL_ERROR", beamdrop::app::ErrorCode::ProtocolError)
        .value("FILE_SYSTEM_ERROR", beamdrop::app::ErrorCode::FileSystemError)
        .value("CANCELLED", beamdrop::app::ErrorCode::Cancelled)
        .value("ALREADY_RUNNING", beamdrop::app::ErrorCode::AlreadyRunning)
        .value("NOT_RUNNING", beamdrop::app::ErrorCode::NotRunning)
        .value("BIND_FAILED", beamdrop::app::ErrorCode::BindFailed)
        .value("INTERNAL_ERROR", beamdrop::app::ErrorCode::InternalError);

    py::class_<beamdrop::app::ServiceError>(m, "ServiceError")
        .def_readonly("code", &beamdrop::app::ServiceError::code)
        .def_readonly("message", &beamdrop::app::ServiceError::message);

    py::enum_<beamdrop::app::TransferProgress::Direction>(m, "TransferDirection")
        .value("SEND", beamdrop::app::TransferProgress::Direction::Send)
        .value("RECEIVE", beamdrop::app::TransferProgress::Direction::Receive);

    py::class_<beamdrop::app::SendResult>(m, "SendResult")
        .def_readonly("file_count", &beamdrop::app::SendResult::file_count)
        .def_readonly("total_bytes", &beamdrop::app::SendResult::total_bytes);

    auto beamdrop_error = py::reinterpret_steal<py::object>(
        PyErr_NewException("beamdrop_native.BeamDropError", PyExc_RuntimeError, nullptr));
    m.attr("BeamDropError") = beamdrop_error;

    m.def(
        "send",
        [beamdrop_error](const std::vector<std::string> &paths, const std::string &host, int port,
                         std::size_t chunk_size, py::object on_progress) {
            if (port < 0 || port > 65535) {
                throw py::value_error("port must be in range 0..65535");
            }

            if (!on_progress.is_none() && !PyCallable_Check(on_progress.ptr())) {
                throw py::type_error("on_progress must be callable or None");
            }

            beamdrop::app::SendRequest request;
            request.host = host;
            request.port = port;
            request.chunk_size = chunk_size;
            request.paths.reserve(paths.size());
            for (const auto &path : paths) {
                request.paths.emplace_back(path);
            }

            if (!on_progress.is_none()) {
                py::function callback = py::reinterpret_borrow<py::function>(on_progress);
                request.progress_callback = [callback = std::move(callback)](
                                                const beamdrop::app::TransferProgress &progress) {
                    py::gil_scoped_acquire acquire;
                    try {
                        callback(progress);
                    } catch (py::error_already_set &error) {
                        const std::string message = error.what();
                        error.restore();
                        PyErr_Clear();
                        throw PythonProgressCallbackError(message);
                    }
                };
            }

            auto result = beamdrop::app::ServiceResult<beamdrop::app::SendResult>::success({});
            {
                py::gil_scoped_release release;
                result = beamdrop::app::SendService{}.send(request);
            }

            if (!result) {
                raise_service_error(beamdrop_error, result.error());
            }
            return result.value();
        },
        py::arg("paths"), py::arg("host"), py::arg("port"), py::arg("chunk_size") = 1024 * 1024,
        py::arg("on_progress") = py::none());

    py::enum_<beamdrop::app::ReceiveServerState>(m, "ReceiverState")
        .value("STOPPED", beamdrop::app::ReceiveServerState::Stopped)
        .value("STARTING", beamdrop::app::ReceiveServerState::Starting)
        .value("RUNNING", beamdrop::app::ReceiveServerState::Running)
        .value("STOPPING", beamdrop::app::ReceiveServerState::Stopping)
        .value("FAILED", beamdrop::app::ReceiveServerState::Failed);

    py::class_<beamdrop::app::ReceiveServerStatus>(m, "ReceiverStatus")
        .def_readonly("state", &beamdrop::app::ReceiveServerStatus::state)
        .def_readonly("last_error", &beamdrop::app::ReceiveServerStatus::last_error);

    py::class_<beamdrop::app::ReceiveServerService,
               std::shared_ptr<beamdrop::app::ReceiveServerService>>(m, "ReceiverService")
        .def(py::init<>())
        .def(
            "start",
            [beamdrop_error](beamdrop::app::ReceiveServerService &service, const std::string &host,
                             int port, const std::string &save_dir, const std::string &state_file) {
                if (port < 0 || port > 65535) {
                    throw py::value_error("port must be in range 0..65535");
                }

                beamdrop::app::ReceiveServerRequest request;
                request.host = host;
                request.port = static_cast<std::uint16_t>(port);
                request.receive_request.save_dir = std::filesystem::path{save_dir};
                request.receive_request.state_file = std::filesystem::path{state_file};

                const auto result = service.start(request);
                if (!result) {
                    raise_service_error(beamdrop_error, result.error());
                }
                return result.value();
            },
            py::arg("host"), py::arg("port"), py::arg("save_dir"), py::arg("state_file"))
        .def("status", &beamdrop::app::ReceiveServerService::status)
        .def("stop", [beamdrop_error](beamdrop::app::ReceiveServerService &service) {
            const auto result = service.stop();
            if (!result) {
                raise_service_error(beamdrop_error, result.error());
            }
            return result.value();
        });

    py::enum_<beamdrop::transfer::Stage>(m, "TransferStage")
        .value("TASK_STARTED", beamdrop::transfer::Stage::TaskStarted)
        .value("TRANSFERRING", beamdrop::transfer::Stage::Transferring)
        .value("FILE_COMPLETED", beamdrop::transfer::Stage::FileCompleted)
        .value("TASK_COMPLETED", beamdrop::transfer::Stage::TaskCompleted);

    py::class_<beamdrop::app::TransferProgress>(m, "TransferProgress")
        .def_readonly("direction", &beamdrop::app::TransferProgress::direction)
        .def_readonly("relative_path", &beamdrop::app::TransferProgress::relative_path)
        .def_readonly("current_file_bytes", &beamdrop::app::TransferProgress::current_file_bytes)
        .def_readonly("current_file_total_bytes",
                      &beamdrop::app::TransferProgress::current_file_total_bytes)
        .def_readonly("file_index", &beamdrop::app::TransferProgress::file_index)
        .def_readonly("file_count", &beamdrop::app::TransferProgress::file_count)
        .def_readonly("stage", &beamdrop::app::TransferProgress::stage);
}