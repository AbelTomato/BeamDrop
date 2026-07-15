"""Thread-safe ownership of the singleton native receiver service."""

from __future__ import annotations

from threading import Lock
from time import monotonic, sleep

from app.core.native_gateway import NativeGateway, NativeGatewayError
from app.core.event_bus import EventBus
from app.schemas.contracts import ErrorPayload, ReceiverSnapshot, ReceiverState, StartReceiverRequest
from app.schemas.wire import ReceiverSnapshotDto


class ReceiverAlreadyRunningError(Exception):
    """Raised when an API caller attempts to create a second listener."""


class ReceiverManager:
    _STOP_POLL_SECONDS = 0.025
    _STOP_WAIT_SECONDS = 2.0
    def __init__(self, gateway: NativeGateway, event_bus: EventBus | None = None) -> None:
        self._gateway = gateway
        self._event_bus = event_bus
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
                result = self._snapshot
            self._publish("receiver.started", result)
            return result

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

            # ReceiveServerService.stop() acknowledges the stop request before
            # its accept worker has observed the stop token.  Returning that
            # transient ``stopping`` snapshot as ``receiver.stopped`` leaves
            # the UI permanently disabled because no later native callback
            # updates the manager.  Poll the authoritative native status on
            # this worker-thread route until it reaches a terminal state.
            native_snapshot = self._wait_for_stop(native_snapshot, request)
            with self._lock:
                self._snapshot = native_snapshot
                result = self._snapshot
            self._publish("receiver.stopped" if result.state is ReceiverState.STOPPED else "error", result)
            return result

    def _wait_for_stop(
        self, snapshot: ReceiverSnapshot, request: StartReceiverRequest | None
    ) -> ReceiverSnapshot:
        deadline = monotonic() + self._STOP_WAIT_SECONDS
        while snapshot.state is ReceiverState.STOPPING and monotonic() < deadline:
            sleep(self._STOP_POLL_SECONDS)
            try:
                snapshot = self._gateway.receiver_status()
            except NativeGatewayError as error:
                return ReceiverSnapshot(state=ReceiverState.FAILED, request=request, error=error.error)
            except Exception as error:
                return ReceiverSnapshot(
                    state=ReceiverState.FAILED,
                    request=request,
                    error=ErrorPayload(code="INTERNAL_ERROR", message=str(error)),
                )

        if snapshot.state is ReceiverState.STOPPED:
            return snapshot
        if snapshot.state is ReceiverState.FAILED:
            return ReceiverSnapshot(state=ReceiverState.FAILED, request=request, error=snapshot.error)
        return ReceiverSnapshot(
            state=ReceiverState.FAILED,
            request=request,
            error=ErrorPayload(code="STOP_TIMEOUT", message="receiver service did not stop within 2 seconds"),
        )

    def _set_failed(self, request: StartReceiverRequest | None, error: ErrorPayload) -> ReceiverSnapshot:
        with self._lock:
            self._snapshot = ReceiverSnapshot(state=ReceiverState.FAILED, request=request, error=error)
            result = self._snapshot
        self._publish("error", result)
        return result

    def _publish(self, event_type: str, snapshot: ReceiverSnapshot) -> None:
        if self._event_bus is not None:
            # WebSocket events use the same flattened receiver DTO as REST.
            # Publishing the domain snapshot directly would expose its nested
            # ``request`` object, while the frontend contract expects host,
            # port and save_dir at the receiver payload's top level.
            if event_type == "error":
                self._event_bus.publish("error", payload={"error": snapshot.error})
                return
            self._event_bus.publish(
                event_type,
                payload={"receiver": ReceiverSnapshotDto.from_domain(snapshot).model_dump(mode="json")},
            )