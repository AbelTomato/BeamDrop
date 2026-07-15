"""Learner exercise: own the send-task lifecycle here.

Implement the public methods without changing the gateway contract. The tests
in ``backend/tests/test_task_manager.py`` define the required behavior.
"""

from __future__ import annotations

import asyncio
import inspect

from app.core.native_gateway import NativeGateway, NativeGatewayError
from app.schemas.contracts import ErrorPayload, SendRequest, TaskCreated, TaskSnapshot, TaskState
from datetime import datetime, timezone
from uuid import uuid4
from dataclasses import replace
from threading import Lock
from threading import Event

from app.core.event_bus import EventBus


class TaskManager:
    def __init__(self, gateway: NativeGateway, event_bus: EventBus | None = None) -> None:
        self._gateway = gateway
        self._event_bus = event_bus
        self._tasks: dict[str, TaskSnapshot] = {}
        self._cancel_events: dict[str, Event] = {}
        self._lock = Lock()

    def create(self, request: SendRequest) -> TaskCreated:
        with self._lock:
            task_id = str(uuid4())
            now = datetime.now(timezone.utc)

            snapshot = TaskSnapshot(
                task_id=task_id,
                state=TaskState.PENDING,
                request=request,
                created_at=now,
                started_at=None,
                finished_at=None,
                progress=None,
                error=None
            )

            self._tasks[task_id] = snapshot
            self._cancel_events[task_id] = Event()
        self._publish("task.created", task_id, {"task": snapshot})
        return TaskCreated(task_id=task_id, created_at=now)

    def get(self, task_id: str) -> TaskSnapshot | None:
        with self._lock:
            return self._tasks.get(task_id)

    def snapshots(self) -> tuple[TaskSnapshot, ...]:
        with self._lock:
            return tuple(self._tasks.values())

    async def run(self, task_id: str) -> None:
        with self._lock:
            task = self._tasks.get(task_id)
            if task is None:
                return
            if task.state is not TaskState.PENDING:
                return
            running = replace(
                task,
                state=TaskState.RUNNING,
                started_at=datetime.now(timezone.utc)
            )
            self._tasks[task_id] = running
        self._publish("task.started", task_id, {"task": running})


        def on_progress(progress) -> None:
            with self._lock:
                current = self._tasks.get(task_id)
                if current is not None and current.state is TaskState.RUNNING:
                    updated = replace(current, progress=progress)
                    self._tasks[task_id] = updated
                else:
                    return
            self._publish("transfer.progress", task_id, {"progress": progress})

        try:
            send_parameters = inspect.signature(self._gateway.send).parameters
            args = (running.request, on_progress)
            if "cancel_event" in send_parameters:
                args += (self._cancel_events[task_id],)
            result = await asyncio.to_thread(self._gateway.send, *args)
        except NativeGatewayError as error:
            with self._lock:
                current = self._tasks.get(task_id)
                if current is not None and current.state in (TaskState.RUNNING, TaskState.CANCELING):
                    self._tasks[task_id] = replace(
                        current,
                        state=TaskState.CANCELED if error.error.code == "CANCELLED" else TaskState.FAILED,
                        error=None if error.error.code == "CANCELLED" else error.error,
                        finished_at=datetime.now(timezone.utc)
                    )
                    failed = self._tasks[task_id]
                else:
                    return
            self._publish("task.canceled" if failed.state is TaskState.CANCELED else "task.failed", task_id, {"task": failed})
        except Exception as error:
            with self._lock:
                current = self._tasks.get(task_id)
                if current is not None and current.state in (TaskState.RUNNING, TaskState.CANCELING):
                    self._tasks[task_id] = replace(
                        current,
                        state=TaskState.FAILED,
                        error=ErrorPayload(code="INTERNAL_ERROR", message=str(error)),
                        finished_at=datetime.now(timezone.utc)
                    )
                    failed = self._tasks[task_id]
                else:
                    return
            self._publish("task.failed", task_id, {"task": failed})
        else:
            with self._lock:
                current = self._tasks.get(task_id)
                if current is not None and current.state is TaskState.RUNNING:
                    final_progress = current.progress
                    if final_progress is not None and result.total_bytes > 0:
                        final_progress = replace(
                            final_progress,
                            current_file_bytes=result.total_bytes,
                            current_file_total_bytes=result.total_bytes,
                            total_bytes=result.total_bytes,
                            transferred_bytes=result.total_bytes,
                        )
                    self._tasks[task_id] = replace(
                        current,
                        state=TaskState.COMPLETED,
                        progress=final_progress,
                        finished_at=datetime.now(timezone.utc)
                    )
                    completed = self._tasks[task_id]
                elif current is not None and current.state is TaskState.CANCELING:
                    # A cancel request is cooperative.  A successful native
                    # return proves the transfer reached its normal end, even
                    # when the request raced with its final cancellation check.
                    self._tasks[task_id] = replace(
                        current,
                        state=TaskState.COMPLETED,
                        finished_at=datetime.now(timezone.utc),
                    )
                    completed = self._tasks[task_id]
                else:
                    return
            self._publish("task.completed", task_id, {"task": completed})

    def cancel(self, task_id: str) -> TaskSnapshot | None:
        with self._lock:
            task = self._tasks.get(task_id)
            if task is None or task.state not in (TaskState.PENDING, TaskState.RUNNING):
                return task
            self._cancel_events[task_id].set()
            updated = replace(task, state=TaskState.CANCELING) if task.state is TaskState.RUNNING else replace(
                task, state=TaskState.CANCELED, finished_at=datetime.now(timezone.utc))
            self._tasks[task_id] = updated
        self._publish("task.canceling" if updated.state is TaskState.CANCELING else "task.canceled", task_id, {"task": updated})
        return updated

    def _publish(self, event_type: str, task_id: str, payload: dict[str, object]) -> None:
        if self._event_bus is not None:
            self._event_bus.publish(event_type, task_id=task_id, payload=payload)