"""Learner exercise: own the send-task lifecycle here.

Implement the public methods without changing the gateway contract. The tests
in ``backend/tests/test_task_manager.py`` define the required behavior.
"""

from __future__ import annotations

import asyncio

from app.core.native_gateway import NativeGateway, NativeGatewayError
from app.schemas.contracts import ErrorPayload, SendRequest, TaskCreated, TaskSnapshot, TaskState
from datetime import datetime, timezone
from uuid import uuid4
from dataclasses import replace
from threading import Lock


class TaskManager:
    def __init__(self, gateway: NativeGateway) -> None:
        self._gateway = gateway
        self._tasks: dict[str, TaskSnapshot] = {}
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


        def on_progress(progress) -> None:
            with self._lock:
                current = self._tasks.get(task_id)
                if current is not None and current.state is TaskState.RUNNING:
                    self._tasks[task_id] = replace(current, progress=progress)

        try:
            await asyncio.to_thread(
                self._gateway.send,
                running.request,
                on_progress
            )
        except NativeGatewayError as error:
            with self._lock:
                current = self._tasks.get(task_id)
                if current is not None and current.state is TaskState.RUNNING:
                    self._tasks[task_id] = replace(
                        current,
                        state=TaskState.FAILED,
                        error=error.error,
                        finished_at=datetime.now(timezone.utc)
                    )
        except Exception as error:
            with self._lock:
                current = self._tasks.get(task_id)
                if current is not None and current.state is TaskState.RUNNING:
                    self._tasks[task_id] = replace(
                        current,
                        state=TaskState.FAILED,
                        error=ErrorPayload(code="INTERNAL_ERROR", message=str(error)),
                        finished_at=datetime.now(timezone.utc)
                    )
        else:
            with self._lock:
                current = self._tasks.get(task_id)
                if current is not None and current.state is TaskState.RUNNING:
                    self._tasks[task_id] = replace(
                        current,
                        state=TaskState.COMPLETED,
                        finished_at=datetime.now(timezone.utc)
                    )