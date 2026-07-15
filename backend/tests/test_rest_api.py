from __future__ import annotations

import time

from fastapi.testclient import TestClient

from app.core.fake_native_gateway import FakeNativeGateway
from app.main import create_app
from app.schemas.contracts import ErrorPayload


def client_for(gateway: FakeNativeGateway | None = None) -> TestClient:
    return TestClient(create_app(gateway or FakeNativeGateway()))


def send_payload() -> dict[str, object]:
    return {
        "source_path": "source.bin",
        "target_host": "127.0.0.1",
        "target_port": 9000,
        "chunk_size": 4096,
    }


def receiver_payload() -> dict[str, object]:
    return {"host": "127.0.0.1", "port": 9000, "save_dir": "downloads"}


def test_health_and_empty_snapshot_are_available() -> None:
    client = client_for()

    assert client.get("/api/health").json() == {"status": "ok"}
    snapshot = client.get("/api/snapshot")

    assert snapshot.status_code == 200
    assert snapshot.json()["tasks"] == []
    assert snapshot.json()["receiver"] == {
        "state": "stopped",
        "host": None,
        "port": None,
        "save_dir": None,
        "last_error": None,
    }


def test_send_returns_accepted_task_and_snapshot_exposes_terminal_result() -> None:
    client = client_for()

    created = client.post("/api/transfers/send", json=send_payload())

    assert created.status_code == 202
    task_id = created.json()["task_id"]
    task = _wait_for_task(client, task_id)
    assert task["state"] == "completed"
    assert task["request"] == send_payload()


def test_send_validation_returns_stable_error_body() -> None:
    client = client_for()
    payload = send_payload()
    payload["target_port"] = 0

    response = client.post("/api/transfers/send", json=payload)

    assert response.status_code == 422
    assert response.json()["error"]["code"] == "VALIDATION_ERROR"
    assert response.json()["error"]["details"]["issues"]


def test_send_accepts_directory_source_path() -> None:
    client = client_for()
    payload = send_payload()
    payload["source_path"] = "."

    created = client.post("/api/transfers/send", json=payload)

    assert created.status_code == 202
    task = _wait_for_task(client, created.json()["task_id"])
    assert task["state"] == "completed"
    assert task["request"] == payload


def test_cors_only_allows_configured_vite_origins() -> None:
    client = client_for()

    allowed = client.options(
        "/api/health",
        headers={"Origin": "http://127.0.0.1:5173", "Access-Control-Request-Method": "GET"},
    )
    rejected = client.options(
        "/api/health",
        headers={"Origin": "http://example.invalid", "Access-Control-Request-Method": "GET"},
    )

    assert allowed.status_code == 200
    assert allowed.headers["access-control-allow-origin"] == "http://127.0.0.1:5173"
    assert rejected.status_code == 400


def test_native_failure_is_visible_in_authoritative_task_snapshot() -> None:
    gateway = FakeNativeGateway(error=ErrorPayload(code="NETWORK_FAILURE", message="connection refused"))
    client = client_for(gateway)

    created = client.post("/api/transfers/send", json=send_payload())
    task = _wait_for_task(client, created.json()["task_id"])

    assert task["state"] == "failed"
    assert task["error"] == {"code": "NETWORK_FAILURE", "message": "connection refused", "details": None}


def test_receiver_start_stop_and_duplicate_start_have_deterministic_responses() -> None:
    client = client_for()

    started = client.post("/api/receiver/start", json=receiver_payload())
    duplicate = client.post("/api/receiver/start", json=receiver_payload())
    stopped = client.post("/api/receiver/stop")

    assert started.status_code == 200
    assert started.json()["state"] == "running"
    assert duplicate.status_code == 409
    assert duplicate.json()["error"]["code"] == "RECEIVER_ALREADY_RUNNING"
    assert stopped.status_code == 200
    assert stopped.json()["state"] == "stopped"


def test_missing_task_returns_structured_404() -> None:
    response = client_for().get("/api/tasks/not-a-real-task")

    assert response.status_code == 404
    assert response.json()["error"]["code"] == "TASK_NOT_FOUND"


def _wait_for_task(client: TestClient, task_id: str) -> dict[str, object]:
    deadline = time.monotonic() + 1
    while time.monotonic() < deadline:
        response = client.get(f"/api/tasks/{task_id}")
        body = response.json()
        if body["state"] in {"completed", "failed"}:
            return body
        time.sleep(0.01)
    raise AssertionError(f"task {task_id} did not reach a terminal state")