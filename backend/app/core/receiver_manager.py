"""Thread-safe ownership of the singleton native receiver service."""

from __future__ import annotations

from threading import Lock

from app.core.native_gateway import NativeGateway, NativeGatewayError
from app.schemas.contracts import ErrorPayload, ReceiverSnapshot, ReceiverState, StartReceiverRequest


class ReceiverAlreadyRunningError(Exception):
    """Raised when an API caller attempts to create a second listener."""


class ReceiverManager:
    def __init__(self, gateway: NativeGateway) -> None:
        self._gateway = gateway
        self._lock = Lock()
        self._operation_lock = Lock()
        self._snapshot = ReceiverSnapshot(state=ReceiverState.STOPPED, request=None, error=None)

    def snapshot(self) -> ReceiverSnapshot:
        with self._lock:
            return self._snapshot

    def start(self, request: StartReceiverRequest) -> ReceiverSnapshot:
        with self._operation_lock:
            with self._lock:
                if self._snapshot.state in (ReceiverState.STARTING, ReceiverState.RUNNING, ReceiverState.STOPPING):
                    raise ReceiverAlreadyRunningError("receiver service is already active")
                self._snapshot = ReceiverSnapshot(state=ReceiverState.STARTING, request=request, error=None)

            try:
                native_snapshot = self._gateway.start_receiver(request)
            except NativeGatewayError as error:
                return self._set_failed(request, error.error)
            except Exception as error:
                return self._set_failed(request, ErrorPayload(code="INTERNAL_ERROR", message=str(error)))

            with self._lock:
                self._snapshot = native_snapshot
                return self._snapshot

    def stop(self) -> ReceiverSnapshot:
        with self._operation_lock:
            with self._lock:
                if self._snapshot.state is ReceiverState.STOPPED:
                    return self._snapshot
                request = self._snapshot.request
                self._snapshot = ReceiverSnapshot(state=ReceiverState.STOPPING, request=request, error=None)

            try:
                native_snapshot = self._gateway.stop_receiver()
            except NativeGatewayError as error:
                return self._set_failed(request, error.error)
            except Exception as error:
                return self._set_failed(request, ErrorPayload(code="INTERNAL_ERROR", message=str(error)))

            with self._lock:
                self._snapshot = native_snapshot
                return self._snapshot

    def _set_failed(self, request: StartReceiverRequest | None, error: ErrorPayload) -> ReceiverSnapshot:
        with self._lock:
            self._snapshot = ReceiverSnapshot(state=ReceiverState.FAILED, request=request, error=error)
            return self._snapshot