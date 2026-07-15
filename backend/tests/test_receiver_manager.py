from __future__ import annotations

from threading import Event, Thread

import pytest

from app.core.fake_native_gateway import FakeNativeGateway
from app.core.receiver_manager import ReceiverAlreadyRunningError, ReceiverManager
from app.schemas.contracts import ErrorPayload, ReceiverState, StartReceiverRequest


def receiver_request() -> StartReceiverRequest:
    return StartReceiverRequest(
        host="127.0.0.1",
        port=9000,
        save_dir="downloads",
        state_file="receiver-state.json",
    )


def test_start_records_running_snapshot_and_uses_gateway_once() -> None:
    gateway = FakeNativeGateway()
    manager = ReceiverManager(gateway)

    snapshot = manager.start(receiver_request())

    assert snapshot.state is ReceiverState.RUNNING
    assert snapshot.request == receiver_request()
    assert snapshot.error is None
    assert manager.snapshot() == snapshot
    assert gateway.receiver_start_calls == [receiver_request()]


def test_duplicate_start_returns_deterministic_error_without_second_listener() -> None:
    gateway = FakeNativeGateway()
    manager = ReceiverManager(gateway)
    manager.start(receiver_request())

    with pytest.raises(ReceiverAlreadyRunningError):
        manager.start(receiver_request())

    assert len(gateway.receiver_start_calls) == 1
    assert manager.snapshot().state is ReceiverState.RUNNING


def test_stop_transitions_to_stopped_and_is_idempotent() -> None:
    gateway = FakeNativeGateway()
    manager = ReceiverManager(gateway)
    manager.start(receiver_request())

    stopped = manager.stop()
    repeated_stop = manager.stop()

    assert stopped.state is ReceiverState.STOPPED
    assert repeated_stop.state is ReceiverState.STOPPED
    assert gateway.receiver_stop_calls == 1


def test_stop_waits_for_native_stopping_state_to_settle_before_publishing_stopped() -> None:
    class AsyncStopGateway(FakeNativeGateway):
        def __init__(self) -> None:
            super().__init__()
            self.status_calls = 0

        def stop_receiver(self):
            self.receiver_stop_calls += 1
            self.receiver_snapshot = self.receiver_snapshot.__class__(
                state=ReceiverState.STOPPING, request=self.receiver_snapshot.request, error=None
            )
            return self.receiver_snapshot

        def receiver_status(self):
            self.status_calls += 1
            if self.status_calls == 2:
                self.receiver_snapshot = self.receiver_snapshot.__class__(
                    state=ReceiverState.STOPPED, request=None, error=None
                )
            return self.receiver_snapshot

    gateway = AsyncStopGateway()
    manager = ReceiverManager(gateway)
    manager.start(receiver_request())

    stopped = manager.stop()

    assert stopped.state is ReceiverState.STOPPED
    assert gateway.status_calls == 2


def test_start_native_error_becomes_failed_snapshot() -> None:
    error = ErrorPayload(code="ADDRESS_IN_USE", message="port is occupied")
    gateway = FakeNativeGateway()
    gateway.receiver_start_error = error
    manager = ReceiverManager(gateway)

    snapshot = manager.start(receiver_request())

    assert snapshot.state is ReceiverState.FAILED
    assert snapshot.request == receiver_request()
    assert snapshot.error == error


def test_stop_native_error_becomes_failed_snapshot_with_original_request() -> None:
    error = ErrorPayload(code="NETWORK_FAILURE", message="listener stop failed")
    gateway = FakeNativeGateway()
    gateway.receiver_stop_error = error
    manager = ReceiverManager(gateway)
    manager.start(receiver_request())

    snapshot = manager.stop()

    assert snapshot.state is ReceiverState.FAILED
    assert snapshot.request == receiver_request()
    assert snapshot.error == error


def test_stop_waits_for_start_without_overwriting_native_stop_result() -> None:
    release_start = Event()
    start_entered = Event()
    gateway = FakeNativeGateway()
    gateway.receiver_start_block_until = release_start
    gateway.on_receiver_start = start_entered.set
    manager = ReceiverManager(gateway)
    start_result: list[object] = []
    stop_result: list[object] = []

    start_thread = Thread(target=lambda: start_result.append(manager.start(receiver_request())))
    stop_thread = Thread(target=lambda: stop_result.append(manager.stop()))
    start_thread.start()
    assert start_entered.wait(timeout=1)
    stop_thread.start()
    release_start.set()
    start_thread.join(timeout=1)
    stop_thread.join(timeout=1)

    assert not start_thread.is_alive()
    assert not stop_thread.is_alive()
    assert start_result[0].state is ReceiverState.RUNNING
    assert stop_result[0].state is ReceiverState.STOPPED
    assert manager.snapshot().state is ReceiverState.STOPPED