"""Boundary between Python application services and native bindings."""

from __future__ import annotations

from collections.abc import Callable
from typing import Protocol

from app.schemas.contracts import (
    ErrorPayload,
    NativeSendResult,
    ReceiverSnapshot,
    ReceiverState,
    SendRequest,
    StartReceiverRequest,
    TransferDirection,
    TransferProgress,
    TransferStage,
)

ProgressCallback = Callable[[TransferProgress], None]


class NativeGatewayError(Exception):
    """A stable native failure that application state can expose to callers."""

    def __init__(self, error: ErrorPayload) -> None:
        super().__init__(error.message)
        self.error = error


class NativeGateway(Protocol):
    def send(self, request: SendRequest, on_progress: ProgressCallback) -> NativeSendResult:
        """Execute the blocking native send operation on a worker thread."""

    def start_receiver(self, request: StartReceiverRequest) -> ReceiverSnapshot:
        """Start the singleton native receiver service."""

    def receiver_status(self) -> ReceiverSnapshot:
        """Return the native receiver's current status."""

    def stop_receiver(self) -> ReceiverSnapshot:
        """Stop the singleton native receiver service."""


class PybindNativeGateway:
    """Thin adapter over ``beamdrop_native``; no task state belongs here."""

    def __init__(self, native_module: object | None = None) -> None:
        if native_module is None:
            try:
                import beamdrop_native  # type: ignore[import-not-found]
            except ImportError as error:
                raise RuntimeError("beamdrop_native is unavailable; configure PYTHONPATH for the built module") from error
            native_module = beamdrop_native
        self._native = native_module

    def send(self, request: SendRequest, on_progress: ProgressCallback) -> NativeSendResult:
        try:
            native_result = self._native.send(
                [request.source_path],
                request.target_host,
                request.target_port,
                request.chunk_size or 1024 * 1024,
                lambda native_progress: on_progress(self._to_progress(native_progress)),
            )
        except Exception as error:
            raise self._to_gateway_error(error) from error

        return NativeSendResult(file_count=native_result.file_count, total_bytes=native_result.total_bytes)

    def start_receiver(self, request: StartReceiverRequest) -> ReceiverSnapshot:
        try:
            status = self._receiver_service().start(request.host, request.port, request.save_dir, request.state_file)
            return self._to_receiver_snapshot(status, request)
        except Exception as error:
            raise self._to_gateway_error(error) from error

    def receiver_status(self) -> ReceiverSnapshot:
        try:
            return self._to_receiver_snapshot(self._receiver_service().status(), None)
        except Exception as error:
            raise self._to_gateway_error(error) from error

    def stop_receiver(self) -> ReceiverSnapshot:
        try:
            return self._to_receiver_snapshot(self._receiver_service().stop(), None)
        except Exception as error:
            raise self._to_gateway_error(error) from error

    def _receiver_service(self) -> object:
        service = getattr(self, "_receiver", None)
        if service is None:
            service = self._native.ReceiverService()
            self._receiver = service
        return service

    @staticmethod
    def _to_gateway_error(error: Exception) -> NativeGatewayError:
        native_error = getattr(error, "code", "INTERNAL_ERROR")
        code = getattr(native_error, "name", str(native_error))
        return NativeGatewayError(ErrorPayload(code=code, message=str(error)))

    @staticmethod
    def _to_receiver_snapshot(value: object, request: StartReceiverRequest | None) -> ReceiverSnapshot:
        state = ReceiverState(value.state.name.lower())
        native_error = getattr(value, "last_error", None)
        error = None
        if native_error is not None:
            error = ErrorPayload(code=native_error.code.name, message=native_error.message)
        return ReceiverSnapshot(state=state, request=request, error=error)

    def _to_progress(self, value: object) -> TransferProgress:
        direction = TransferDirection.SEND if value.direction.name == "SEND" else TransferDirection.RECEIVE
        return TransferProgress(
            direction=direction,
            stage=TransferStage(value.stage.name.lower()),
            file_index=value.file_index,
            file_count=value.file_count,
            relative_path=value.relative_path,
            current_file_bytes=value.current_file_bytes,
            current_file_total_bytes=value.current_file_total_bytes,
        )