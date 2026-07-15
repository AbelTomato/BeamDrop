"""Deterministic fake native boundary for TaskManager unit tests."""

from __future__ import annotations

from collections.abc import Callable
from threading import Event

from app.core.native_gateway import NativeGatewayError, ProgressCallback
from app.schemas.contracts import (
    ErrorPayload,
    NativeSendResult,
    ReceiverSnapshot,
    ReceiverState,
    SendRequest,
    StartReceiverRequest,
    TransferProgress,
)


class FakeNativeGateway:
    def __init__(
        self,
        *,
        result: NativeSendResult = NativeSendResult(file_count=1, total_bytes=0),
        error: ErrorPayload | None = None,
        block_until: Event | None = None,
    ) -> None:
        self.result = result
        self.error = error
        self.block_until = block_until
        self.calls: list[SendRequest] = []
        self.on_send_started: Callable[[], None] | None = None
        self._progress_callbacks: list[ProgressCallback] = []
        self.receiver_snapshot = ReceiverSnapshot(state=ReceiverState.STOPPED, request=None, error=None)
        self.receiver_start_error: ErrorPayload | None = None
        self.receiver_stop_error: ErrorPayload | None = None
        self.receiver_start_calls: list[StartReceiverRequest] = []
        self.receiver_stop_calls = 0
        self.receiver_start_block_until: Event | None = None
        self.on_receiver_start: Callable[[], None] | None = None

    def send(self, request: SendRequest, on_progress: ProgressCallback, cancel_event: Event | None = None) -> NativeSendResult:
        self.calls.append(request)
        self._progress_callbacks.append(on_progress)
        if self.on_send_started is not None:
            self.on_send_started()
        if self.block_until is not None:
            while not self.block_until.wait(timeout=0.02):
                if cancel_event is not None and cancel_event.is_set():
                    raise NativeGatewayError(ErrorPayload(code="CANCELLED", message="send cancelled"))
        if self.error is not None:
            raise NativeGatewayError(self.error)
        return self.result

    def emit_progress(self, progress: TransferProgress) -> None:
        for callback in tuple(self._progress_callbacks):
            callback(progress)

    def start_receiver(self, request: StartReceiverRequest) -> ReceiverSnapshot:
        self.receiver_start_calls.append(request)
        if self.on_receiver_start is not None:
            self.on_receiver_start()
        if self.receiver_start_block_until is not None:
            self.receiver_start_block_until.wait(timeout=5)
        if self.receiver_start_error is not None:
            raise NativeGatewayError(self.receiver_start_error)
        self.receiver_snapshot = ReceiverSnapshot(state=ReceiverState.RUNNING, request=request, error=None)
        return self.receiver_snapshot

    def receiver_status(self) -> ReceiverSnapshot:
        return self.receiver_snapshot

    def stop_receiver(self) -> ReceiverSnapshot:
        self.receiver_stop_calls += 1
        if self.receiver_stop_error is not None:
            raise NativeGatewayError(self.receiver_stop_error)
        self.receiver_snapshot = ReceiverSnapshot(state=ReceiverState.STOPPED, request=None, error=None)
        return self.receiver_snapshot