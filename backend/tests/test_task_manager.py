"""Red tests for the TaskManager learner exercise.

Remove one ``xfail`` marker at a time while implementing its corresponding
behavior. The markers keep the repository test suite green between handoffs.
"""

from __future__ import annotations

import asyncio
from threading import Event

import pytest

from app.core.fake_native_gateway import FakeNativeGateway
from app.core.task_manager import TaskManager
from app.schemas.contracts import (
    ErrorPayload,
    NativeSendResult,
    SendRequest,
    TaskState,
    TransferDirection,
    TransferProgress,
    TransferStage,
)


def send_request() -> SendRequest:
    return SendRequest(source_path="source.bin", target_host="127.0.0.1", target_port=9000)


def test_create_returns_pending_task_with_host_generated_id() -> None:
    manager = TaskManager(FakeNativeGateway())

    created = manager.create(send_request())
    task = manager.get(created.task_id)

    assert task is not None
    assert task.state is TaskState.PENDING
    assert task.request == send_request()
    assert task.started_at is None
    assert task.finished_at is None


def test_run_transitions_pending_task_to_completed_without_blocking_event_loop() -> None:
    release = Event()
    gateway = FakeNativeGateway(block_until=release)
    started = Event()
    gateway.on_send_started = started.set
    manager = TaskManager(gateway)
    task_id = manager.create(send_request()).task_id

    async def scenario() -> None:
        running = asyncio.create_task(manager.run(task_id))
        assert await asyncio.to_thread(started.wait, 1)
        assert manager.get(task_id).state is TaskState.RUNNING
        # This yield only completes if ``gateway.send`` is running outside the event loop.
        await asyncio.sleep(0)
        release.set()
        await running

    asyncio.run(scenario())
    assert manager.get(task_id).state is TaskState.COMPLETED
    assert len(gateway.calls) == 1

def test_progress_callback_updates_running_task_snapshot() -> None:
    release = Event()
    gateway = FakeNativeGateway(block_until=release)
    started = Event()
    gateway.on_send_started = started.set
    manager = TaskManager(gateway)
    task_id = manager.create(send_request()).task_id

    progress = TransferProgress(
        direction=TransferDirection.SEND,
        stage=TransferStage.TRANSFERRING,
        file_index=1,
        file_count=1,
        relative_path="source.bin",
        current_file_bytes=50,
        current_file_total_bytes=100,
    )

    async def scenario() -> None:
        running = asyncio.create_task(manager.run(task_id))
        assert await asyncio.to_thread(started.wait, 1)

        gateway.emit_progress(progress)

        task = manager.get(task_id)
        assert task is not None
        assert task.state is TaskState.RUNNING
        assert task.progress == progress

        release.set()
        await running

    asyncio.run(scenario())

    task = manager.get(task_id)
    assert task is not None
    assert task.state is TaskState.COMPLETED
    assert task.progress == progress

def test_running_same_task_twice_only_calls_gateway_once() -> None:
    release = Event()
    gateway = FakeNativeGateway(block_until=release)
    started = Event()
    gateway.on_send_started = started.set
    manager = TaskManager(gateway)
    task_id = manager.create(send_request()).task_id

    async def scenario() -> None:
        tasks = []
        first = asyncio.create_task(manager.run(task_id))
        tasks.append(first)
        await asyncio.to_thread(started.wait, 1)
        for _ in range(100):
            new_task = asyncio.create_task(manager.run(task_id))
            tasks.append(new_task)
        await asyncio.sleep(0)
        release.set()
        await asyncio.gather(*tasks)

    asyncio.run(scenario())
    assert len(gateway.calls) == 1
    assert manager.get(task_id).state is TaskState.COMPLETED

def test_native_error_reaches_one_failed_terminal_state() -> None:
    error = ErrorPayload(code="NETWORK_FAILURE", message="connection refused")
    manager = TaskManager(FakeNativeGateway(error=error))
    task_id = manager.create(send_request()).task_id

    asyncio.run(manager.run(task_id))

    task = manager.get(task_id)
    assert task.state is TaskState.FAILED
    assert task.error == error
    assert task.finished_at is not None


def test_unexpected_gateway_error_becomes_internal_failed_snapshot() -> None:
    class UnexpectedGateway(FakeNativeGateway):
        def send(self, request: SendRequest, on_progress: object) -> NativeSendResult:
            raise RuntimeError("unexpected native wrapper failure")

    manager = TaskManager(UnexpectedGateway())
    task_id = manager.create(send_request()).task_id

    asyncio.run(manager.run(task_id))

    task = manager.get(task_id)
    assert task is not None
    assert task.state is TaskState.FAILED
    assert task.error == ErrorPayload(code="INTERNAL_ERROR", message="unexpected native wrapper failure")
    assert task.finished_at is not None


def test_late_progress_does_not_overwrite_completed_task() -> None:
    gateway = FakeNativeGateway()
    manager = TaskManager(gateway)
    task_id = manager.create(send_request()).task_id
    asyncio.run(manager.run(task_id))

    gateway.emit_progress(
        TransferProgress(
            direction=TransferDirection.SEND,
            stage=TransferStage.TRANSFERRING,
            file_index=1,
            file_count=1,
            relative_path="source.bin",
            current_file_bytes=10,
            current_file_total_bytes=100,
        )
    )

    task = manager.get(task_id)
    assert task.state is TaskState.COMPLETED
    assert task.progress is None


def test_cancel_running_task_reaches_canceled_terminal_state() -> None:
    release = Event()
    gateway = FakeNativeGateway(block_until=release)
    started = Event()
    gateway.on_send_started = started.set
    manager = TaskManager(gateway)
    task_id = manager.create(send_request()).task_id

    async def scenario() -> None:
        running = asyncio.create_task(manager.run(task_id))
        assert await asyncio.to_thread(started.wait, 1)
        canceling = manager.cancel(task_id)
        assert canceling is not None
        assert canceling.state is TaskState.CANCELING
        await running

    asyncio.run(scenario())
    task = manager.get(task_id)
    assert task is not None
    assert task.state is TaskState.CANCELED
    assert task.error is None


def test_cancel_request_racing_with_successful_native_completion_is_completed() -> None:
    release = Event()

    class LateSuccessGateway(FakeNativeGateway):
        def send(self, request: SendRequest, on_progress: object, cancel_event: Event | None = None) -> NativeSendResult:
            self.calls.append(request)
            assert self.block_until is not None
            self.block_until.wait(timeout=1)
            return self.result

    gateway = LateSuccessGateway(block_until=release)
    started = Event()
    gateway.on_send_started = started.set
    manager = TaskManager(gateway)
    task_id = manager.create(send_request()).task_id

    async def scenario() -> None:
        running = asyncio.create_task(manager.run(task_id))
        # The custom gateway does not signal start, so wait until its worker has been entered.
        await asyncio.sleep(0.02)
        assert manager.cancel(task_id).state is TaskState.CANCELING
        release.set()
        await running

    asyncio.run(scenario())
    assert manager.get(task_id).state is TaskState.COMPLETED