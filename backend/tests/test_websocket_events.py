from __future__ import annotations

import asyncio
import time
from threading import Event

from fastapi.testclient import TestClient

from app.core.event_bus import EventBus
from app.core.fake_native_gateway import FakeNativeGateway
from app.main import create_app


def send_payload() -> dict[str, object]:
    return {
        "source_path": "source.bin",
        "target_host": "127.0.0.1",
        "target_port": 9000,
        "chunk_size": 4096,
    }


def test_websocket_receives_ordered_task_lifecycle_events() -> None:
    with TestClient(create_app(FakeNativeGateway())) as client:
        with client.websocket_connect("/ws/events") as websocket:
            created = client.post("/api/transfers/send", json=send_payload())
            task_id = created.json()["task_id"]
            events = [websocket.receive_json() for _ in range(3)]

    assert [event["type"] for event in events] == ["task.created", "task.started", "task.completed"]
    assert [event["sequence"] for event in events] == sorted(event["sequence"] for event in events)
    assert all(event["schema_version"] == 1 for event in events)
    assert all(event["task_id"] == task_id for event in events)


def test_websocket_emits_heartbeat_when_idle() -> None:
    with TestClient(create_app(FakeNativeGateway(), heartbeat_seconds=0.01)) as client:
        with client.websocket_connect("/ws/events") as websocket:
            event = websocket.receive_json()

    assert event["type"] == "heartbeat"
    assert event["schema_version"] == 1
    assert event["task_id"] is None


def test_websocket_receiver_started_uses_the_rest_receiver_dto_shape() -> None:
    with TestClient(create_app(FakeNativeGateway())) as client:
        with client.websocket_connect("/ws/events") as websocket:
            response = client.post(
                "/api/receiver/start",
                json={"host": "127.0.0.1", "port": 9090, "save_dir": "received"},
            )
            event = websocket.receive_json()

    assert response.status_code == 200
    assert event["type"] == "receiver.started"
    assert event["payload"]["receiver"] == {
        "state": "running",
        "host": "127.0.0.1",
        "port": 9090,
        "save_dir": "received",
        "last_error": None,
    }


def test_disconnect_does_not_cancel_background_task() -> None:
    release = Event()
    gateway = FakeNativeGateway(block_until=release)
    with TestClient(create_app(gateway)) as client:
        with client.websocket_connect("/ws/events") as websocket:
            created = client.post("/api/transfers/send", json=send_payload())
            websocket.receive_json()

        release.set()
        task_id = created.json()["task_id"]
        task = _wait_for_task(client, task_id)

    assert task["state"] == "completed"
    assert len(gateway.calls) == 1


def test_slow_subscriber_does_not_block_publishers_and_keeps_terminal_event() -> None:
    async def scenario() -> dict[str, object]:
        bus = EventBus(queue_size=1)
        subscription = await bus.subscribe()
        await asyncio.to_thread(bus.publish, "transfer.progress", task_id="task", payload={"step": 1})
        await asyncio.to_thread(bus.publish, "transfer.progress", task_id="task", payload={"step": 2})
        await asyncio.to_thread(bus.publish, "task.completed", task_id="task", payload={"done": True})
        terminal = await subscription.receive()
        bus.unsubscribe(subscription)
        return terminal

    event = asyncio.run(scenario())

    assert event["type"] == "task.completed"


def _wait_for_task(client: TestClient, task_id: str) -> dict[str, object]:
    deadline = time.monotonic() + 1
    while time.monotonic() < deadline:
        response = client.get(f"/api/tasks/{task_id}")
        body = response.json()
        if body["state"] in {"completed", "failed"}:
            return body
        time.sleep(0.01)
    raise AssertionError(f"task {task_id} did not reach a terminal state")