"""Slice 4 in-memory domain DTOs.

HTTP/Pydantic wire models are intentionally deferred to Slice 5. These types
are owned by the application layer and can therefore be tested without FastAPI.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
from enum import StrEnum


class TaskState(StrEnum):
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"


class ReceiverState(StrEnum):
    STOPPED = "stopped"
    STARTING = "starting"
    RUNNING = "running"
    STOPPING = "stopping"
    FAILED = "failed"


class TransferDirection(StrEnum):
    SEND = "send"
    RECEIVE = "receive"


class TransferStage(StrEnum):
    TASK_STARTED = "task_started"
    TRANSFERRING = "transferring"
    FILE_COMPLETED = "file_completed"
    TASK_COMPLETED = "task_completed"


@dataclass(frozen=True, slots=True)
class ErrorPayload:
    code: str
    message: str
    details: dict[str, object] | None = None


@dataclass(frozen=True, slots=True)
class SendRequest:
    source_path: str
    target_host: str
    target_port: int
    chunk_size: int | None = None


@dataclass(frozen=True, slots=True)
class StartReceiverRequest:
    host: str
    port: int
    save_dir: str
    state_file: str


@dataclass(frozen=True, slots=True)
class ReceiverSnapshot:
    state: ReceiverState
    request: StartReceiverRequest | None
    error: ErrorPayload | None


@dataclass(frozen=True, slots=True)
class TransferProgress:
    direction: TransferDirection
    stage: TransferStage
    file_index: int
    file_count: int
    relative_path: str
    current_file_bytes: int
    current_file_total_bytes: int
    total_bytes: int | None = None
    transferred_bytes: int | None = None


@dataclass(frozen=True, slots=True)
class TaskCreated:
    task_id: str
    created_at: datetime


@dataclass(frozen=True, slots=True)
class TaskSnapshot:
    task_id: str
    state: TaskState
    request: SendRequest
    created_at: datetime
    started_at: datetime | None
    finished_at: datetime | None
    progress: TransferProgress | None
    error: ErrorPayload | None


@dataclass(frozen=True, slots=True)
class NativeSendResult:
    file_count: int
    total_bytes: int