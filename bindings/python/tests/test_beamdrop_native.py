import hashlib
import socket
import time
from pathlib import Path

import pytest

import beamdrop_native as b


def free_loopback_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def wait_for_file(path: Path, timeout_seconds: float = 2.0) -> None:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        if path.exists():
            return
        time.sleep(0.02)
    raise AssertionError(f"received file not found: {path}")


@pytest.fixture
def receiver(tmp_path: Path):
    service = b.ReceiverService()
    port = free_loopback_port()
    received_dir = tmp_path / "received"
    received_dir.mkdir()

    started = service.start(
        "127.0.0.1",
        port,
        str(received_dir),
        str(tmp_path / "receiver-state.json"),
    )
    assert started.state == b.ReceiverState.RUNNING

    try:
        yield service, port, received_dir
    finally:
        status = service.status()
        if status.state != b.ReceiverState.STOPPED:
            service.stop()


def test_send_reports_progress_and_transfers_file(receiver, tmp_path: Path) -> None:
    service, port, received_dir = receiver
    del service  # fixture owns lifetime; prevents an unused-variable warning

    source = tmp_path / "数据.bin"
    source.write_bytes(bytes(range(256)) * 16_384)  # 4 MiB
    stages: list[object] = []

    result = b.send(
        [str(source)],
        "127.0.0.1",
        port,
        on_progress=lambda progress: stages.append(progress.stage),
    )

    assert result.file_count == 1
    assert result.total_bytes == source.stat().st_size
    assert b.TransferStage.TASK_STARTED in stages
    assert b.TransferStage.TRANSFERRING in stages
    assert b.TransferStage.FILE_COMPLETED in stages
    assert b.TransferStage.TASK_COMPLETED in stages

    received = received_dir / source.name
    wait_for_file(received)
    assert hashlib.sha256(received.read_bytes()).digest() == hashlib.sha256(source.read_bytes()).digest()


def test_send_converts_python_callback_exception_to_internal_error(receiver, tmp_path: Path) -> None:
    service, port, _ = receiver
    del service

    source = tmp_path / "数据.bin"
    source.write_bytes(b"callback failure test")

    def failing_callback(_: object) -> None:
        raise RuntimeError("intentional callback failure")

    with pytest.raises(b.BeamDropError) as caught:
        b.send([str(source)], "127.0.0.1", port, on_progress=failing_callback)

    assert caught.value.code == b.ErrorCode.INTERNAL_ERROR


def test_send_rejects_non_callable_progress_handler() -> None:
    with pytest.raises(TypeError, match="on_progress must be callable or None"):
        b.send([], "127.0.0.1", 19090, on_progress=42)
