"""FastAPI REST and WebSocket control-plane application factory."""

from __future__ import annotations

import asyncio

from fastapi import FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect, status
from fastapi.exceptions import RequestValidationError
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

from app.core.native_gateway import NativeGateway, PybindNativeGateway
from app.core.event_bus import EventBus
from app.core.receiver_manager import ReceiverAlreadyRunningError, ReceiverManager
from app.core.task_manager import TaskManager
from app.schemas.wire import (
    AppSnapshotDto,
    ErrorPayloadDto,
    ErrorResponseDto,
    ReceiverSnapshotDto,
    SendRequestDto,
    StartReceiverRequestDto,
    TaskCreatedDto,
    TaskSnapshotDto,
)


def create_app(gateway: NativeGateway | None = None, *, heartbeat_seconds: float = 15.0) -> FastAPI:
    """Create an injectable control-plane app without exposing pybind to routes."""

    native_gateway = gateway or PybindNativeGateway()
    event_bus = EventBus()
    task_manager = TaskManager(native_gateway, event_bus)
    receiver_manager = ReceiverManager(native_gateway, event_bus)
    app = FastAPI(title="BeamDrop Control Plane", version="0.1.0")
    app.state.task_manager = task_manager
    app.state.receiver_manager = receiver_manager
    app.state.event_bus = event_bus
    app.add_middleware(
        CORSMiddleware,
        allow_origins=["http://127.0.0.1:5173", "http://localhost:5173"],
        allow_credentials=False,
        allow_methods=["GET", "POST"],
        allow_headers=["content-type"],
    )

    @app.exception_handler(RequestValidationError)
    async def validation_error(_: Request, error: RequestValidationError) -> JSONResponse:
        return JSONResponse(
            status_code=status.HTTP_422_UNPROCESSABLE_CONTENT,
            content=ErrorResponseDto(
                error=ErrorPayloadDto(
                    code="VALIDATION_ERROR",
                    message="request validation failed",
                    details={"issues": _validation_issues(error)},
                ),
            ).model_dump(mode="json"),
        )

    @app.exception_handler(HTTPException)
    async def http_error(_: Request, error: HTTPException) -> JSONResponse:
        if isinstance(error.detail, dict) and "error" in error.detail:
            return JSONResponse(status_code=error.status_code, content=error.detail)
        return JSONResponse(
            status_code=error.status_code,
            content=ErrorResponseDto(error=ErrorPayloadDto(code="HTTP_ERROR", message=str(error.detail))).model_dump(),
        )

    @app.get("/api/health")
    async def health() -> dict[str, str]:
        return {"status": "ok"}

    @app.get("/api/snapshot", response_model=AppSnapshotDto)
    async def snapshot(request: Request) -> AppSnapshotDto:
        return AppSnapshotDto.from_domain(
            request.app.state.task_manager.snapshots(),
            request.app.state.receiver_manager.snapshot(),
        )

    @app.post("/api/transfers/send", response_model=TaskCreatedDto, status_code=status.HTTP_202_ACCEPTED)
    async def send(payload: SendRequestDto, request: Request) -> TaskCreatedDto:
        manager: TaskManager = request.app.state.task_manager
        created = manager.create(payload.to_domain())
        asyncio.create_task(manager.run(created.task_id))
        return TaskCreatedDto.from_domain(created)

    @app.post("/api/tasks/{task_id}/cancel", response_model=TaskSnapshotDto)
    async def cancel_task(task_id: str, request: Request) -> TaskSnapshotDto:
        task_value = request.app.state.task_manager.cancel(task_id)
        if task_value is None:
            raise _error_response(status.HTTP_404_NOT_FOUND, "TASK_NOT_FOUND", f"task '{task_id}' does not exist")
        return TaskSnapshotDto.from_domain(task_value)

    @app.post("/api/receiver/start", response_model=ReceiverSnapshotDto)
    async def start_receiver(payload: StartReceiverRequestDto, request: Request) -> ReceiverSnapshotDto:
        manager: ReceiverManager = request.app.state.receiver_manager
        try:
            snapshot_value = await asyncio.to_thread(manager.start, payload.to_domain())
        except ReceiverAlreadyRunningError as error:
            raise _error_response(status.HTTP_409_CONFLICT, "RECEIVER_ALREADY_RUNNING", str(error)) from error
        return ReceiverSnapshotDto.from_domain(snapshot_value)

    @app.post("/api/receiver/stop", response_model=ReceiverSnapshotDto)
    async def stop_receiver(request: Request) -> ReceiverSnapshotDto:
        snapshot_value = await asyncio.to_thread(request.app.state.receiver_manager.stop)
        return ReceiverSnapshotDto.from_domain(snapshot_value)

    @app.get("/api/tasks/{task_id}", response_model=TaskSnapshotDto)
    async def task(task_id: str, request: Request) -> TaskSnapshotDto:
        task_value = request.app.state.task_manager.get(task_id)
        if task_value is None:
            raise _error_response(status.HTTP_404_NOT_FOUND, "TASK_NOT_FOUND", f"task '{task_id}' does not exist")
        return TaskSnapshotDto.from_domain(task_value)

    @app.websocket("/ws/events")
    async def events(websocket: WebSocket) -> None:
        await websocket.accept()
        bus: EventBus = websocket.app.state.event_bus
        subscription = await bus.subscribe()
        try:
            while True:
                try:
                    event = await asyncio.wait_for(subscription.receive(), timeout=heartbeat_seconds)
                except TimeoutError:
                    event = bus.heartbeat()
                await websocket.send_json(event)
        except (WebSocketDisconnect, OSError, RuntimeError):
            # Browser development StrictMode may close a just-created socket.
            # A failed send is a normal disconnect, not a server error.
            pass
        finally:
            bus.unsubscribe(subscription)

    return app


def _error_response(status_code: int, code: str, message: str) -> HTTPException:
    return HTTPException(
        status_code=status_code,
        detail=ErrorResponseDto(error=ErrorPayloadDto(code=code, message=message)).model_dump(),
    )


def _validation_issues(error: RequestValidationError) -> list[dict[str, object]]:
    """Return client-safe validation details without serializing Python exceptions."""

    return [
        {
            "location": list(issue["loc"]),
            "message": issue["msg"],
            "type": issue["type"],
        }
        for issue in error.errors()
    ]