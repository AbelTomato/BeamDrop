"""FastAPI wire DTOs for the Slice 5 REST control plane."""

from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
from typing import Annotated, Any

from pydantic import BaseModel, ConfigDict, Field, field_validator

from app.schemas.contracts import (
    ErrorPayload,
    ReceiverSnapshot,
    ReceiverState,
    SendRequest,
    StartReceiverRequest,
    TaskCreated,
    TaskSnapshot,
    TaskState,
    TransferDirection,
    TransferProgress,
    TransferStage,
)

NonEmptyString = Annotated[str, Field(min_length=1)]


class SendRequestDto(BaseModel):
    model_config = ConfigDict(extra="forbid", str_strip_whitespace=True)

    source_path: NonEmptyString
    target_host: NonEmptyString
    target_port: int = Field(ge=1, le=65535)
    chunk_size: int | None = Field(default=None, gt=0)

    @field_validator("source_path")
    @classmethod
    def source_path_must_not_be_directory(cls, value: str) -> str:
        if Path(value).is_dir():
            raise ValueError("source_path must identify a file, not a directory")
        return value

    def to_domain(self) -> SendRequest:
        return SendRequest(**self.model_dump())


class StartReceiverRequestDto(BaseModel):
    model_config = ConfigDict(extra="forbid", str_strip_whitespace=True)

    host: NonEmptyString
    port: int = Field(ge=1, le=65535)
    save_dir: NonEmptyString

    def to_domain(self) -> StartReceiverRequest:
        save_dir = Path(self.save_dir)
        return StartReceiverRequest(
            host=self.host,
            port=self.port,
            save_dir=str(save_dir),
            state_file=str(save_dir / ".beamdrop-receiver-state.json"),
        )


class ErrorPayloadDto(BaseModel):
    code: str
    message: str
    details: dict[str, Any] | None = None

    @classmethod
    def from_domain(cls, value: ErrorPayload) -> "ErrorPayloadDto":
        return cls(code=value.code, message=value.message, details=value.details)


class TransferProgressDto(BaseModel):
    direction: TransferDirection
    stage: TransferStage
    file_index: int
    file_count: int
    relative_path: str
    current_file_bytes: int
    current_file_total_bytes: int
    total_bytes: int | None = None
    transferred_bytes: int | None = None

    @classmethod
    def from_domain(cls, value: TransferProgress) -> "TransferProgressDto":
        return cls(**{
            "direction": value.direction,
            "stage": value.stage,
            "file_index": value.file_index,
            "file_count": value.file_count,
            "relative_path": value.relative_path,
            "current_file_bytes": value.current_file_bytes,
            "current_file_total_bytes": value.current_file_total_bytes,
            "total_bytes": value.total_bytes,
            "transferred_bytes": value.transferred_bytes,
        })


class TaskCreatedDto(BaseModel):
    task_id: str
    created_at: datetime

    @classmethod
    def from_domain(cls, value: TaskCreated) -> "TaskCreatedDto":
        return cls(task_id=value.task_id, created_at=value.created_at)


class TaskSnapshotDto(BaseModel):
    task_id: str
    state: TaskState
    request: SendRequestDto
    created_at: datetime
    started_at: datetime | None
    finished_at: datetime | None
    progress: TransferProgressDto | None
    error: ErrorPayloadDto | None

    @classmethod
    def from_domain(cls, value: TaskSnapshot) -> "TaskSnapshotDto":
        return cls(
            task_id=value.task_id,
            state=value.state,
            request=SendRequestDto(**{
                "source_path": value.request.source_path,
                "target_host": value.request.target_host,
                "target_port": value.request.target_port,
                "chunk_size": value.request.chunk_size,
            }),
            created_at=value.created_at,
            started_at=value.started_at,
            finished_at=value.finished_at,
            progress=TransferProgressDto.from_domain(value.progress) if value.progress else None,
            error=ErrorPayloadDto.from_domain(value.error) if value.error else None,
        )


class ReceiverSnapshotDto(BaseModel):
    state: ReceiverState
    host: str | None
    port: int | None
    save_dir: str | None
    last_error: ErrorPayloadDto | None

    @classmethod
    def from_domain(cls, value: ReceiverSnapshot) -> "ReceiverSnapshotDto":
        request = value.request
        return cls(
            state=value.state,
            host=request.host if request else None,
            port=request.port if request else None,
            save_dir=request.save_dir if request else None,
            last_error=ErrorPayloadDto.from_domain(value.error) if value.error else None,
        )


class AppSnapshotDto(BaseModel):
    tasks: list[TaskSnapshotDto]
    receiver: ReceiverSnapshotDto
    snapshot_at: datetime

    @classmethod
    def from_domain(cls, tasks: tuple[TaskSnapshot, ...], receiver: ReceiverSnapshot) -> "AppSnapshotDto":
        return cls(
            tasks=[TaskSnapshotDto.from_domain(task) for task in tasks],
            receiver=ReceiverSnapshotDto.from_domain(receiver),
            snapshot_at=datetime.now(timezone.utc),
        )


class ErrorResponseDto(BaseModel):
    error: ErrorPayloadDto