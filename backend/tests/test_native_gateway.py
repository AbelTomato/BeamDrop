from __future__ import annotations

from types import SimpleNamespace

import pytest

from app.core.native_gateway import NativeGatewayError, PybindNativeGateway
from app.schemas.contracts import ReceiverState, StartReceiverRequest


class FakeReceiverService:
    def __init__(self) -> None:
        self.calls: list[tuple[object, ...]] = []
        self._status = SimpleNamespace(state=SimpleNamespace(name="STOPPED"), last_error=None)

    def start(self, host: str, port: int, save_dir: str, state_file: str) -> object:
        self.calls.append(("start", host, port, save_dir, state_file))
        self._status = SimpleNamespace(state=SimpleNamespace(name="RUNNING"), last_error=None)
        return self._status

    def status(self) -> object:
        self.calls.append(("status",))
        return self._status

    def stop(self) -> object:
        self.calls.append(("stop",))
        self._status = SimpleNamespace(state=SimpleNamespace(name="STOPPING"), last_error=None)
        return self._status


class FakeNativeModule:
    def __init__(self) -> None:
        self.receiver_service = FakeReceiverService()
        self.receiver_service_creations = 0

    def ReceiverService(self) -> FakeReceiverService:
        self.receiver_service_creations += 1
        return self.receiver_service


def receiver_request() -> StartReceiverRequest:
    return StartReceiverRequest("127.0.0.1", 9000, "downloads", "receiver-state.json")


def test_pybind_gateway_keeps_one_receiver_service_and_normalizes_statuses() -> None:
    native = FakeNativeModule()
    gateway = PybindNativeGateway(native)

    started = gateway.start_receiver(receiver_request())
    status = gateway.receiver_status()
    stopped = gateway.stop_receiver()

    assert started.state is ReceiverState.RUNNING
    assert started.request == receiver_request()
    assert status.state is ReceiverState.RUNNING
    assert status.request is None
    assert stopped.state is ReceiverState.STOPPING
    assert native.receiver_service_creations == 1
    assert native.receiver_service.calls == [
        ("start", "127.0.0.1", 9000, "downloads", "receiver-state.json"),
        ("status",),
        ("stop",),
    ]


def test_pybind_gateway_normalizes_native_receiver_error() -> None:
    class FailingReceiverService(FakeReceiverService):
        def start(self, host: str, port: int, save_dir: str, state_file: str) -> object:
            error = RuntimeError("listener already exists")
            error.code = SimpleNamespace(name="ALREADY_RUNNING")
            raise error

    native = FakeNativeModule()
    native.receiver_service = FailingReceiverService()

    with pytest.raises(NativeGatewayError) as raised:
        PybindNativeGateway(native).start_receiver(receiver_request())

    assert raised.value.error.code == "ALREADY_RUNNING"
    assert raised.value.error.message == "listener already exists"