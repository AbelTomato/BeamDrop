"""Thread-safe, best-effort WebSocket event fan-out.

Business state remains in TaskManager and ReceiverManager. This bus only
delivers incremental notifications to currently connected clients.
"""

from __future__ import annotations

import asyncio
from collections import deque
from collections.abc import Mapping
from dataclasses import dataclass
from datetime import datetime, timezone
from threading import Lock
from typing import Any
from uuid import uuid4

from fastapi.encoders import jsonable_encoder


TERMINAL_EVENT_TYPES = frozenset({"task.completed", "task.failed", "task.canceled", "receiver.stopped", "error"})


@dataclass(slots=True, eq=False)
class EventSubscription:
    loop: asyncio.AbstractEventLoop
    queue: deque[dict[str, Any]]
    ready: asyncio.Event
    lock: Lock

    async def receive(self) -> dict[str, Any]:
        while True:
            with self.lock:
                if self.queue:
                    return self.queue.popleft()
                self.ready.clear()
                # A producer can enqueue between the empty check and clear().
                # Re-check under the same lock before waiting for the next signal.
                if self.queue:
                    self.ready.set()
                    continue
            await self.ready.wait()

    def offer(self, event: dict[str, Any], queue_size: int) -> None:
        with self.lock:
            if len(self.queue) >= queue_size:
                if event["type"] not in TERMINAL_EVENT_TYPES:
                    return
                for index, queued in enumerate(self.queue):
                    if queued["type"] not in TERMINAL_EVENT_TYPES:
                        del self.queue[index]
                        break
                else:
                    # Terminal events are rare and must not be discarded.
                    # Allowing this exceptional overflow prevents a lost final state.
                    pass
            self.queue.append(event)
        self.ready.set()


class EventBus:
    """Assign a global sequence and hand off events without blocking publishers."""

    def __init__(self, *, queue_size: int = 64) -> None:
        self._queue_size = queue_size
        self._lock = Lock()
        self._sequence = 0
        self._subscriptions: set[EventSubscription] = set()

    async def subscribe(self) -> EventSubscription:
        subscription = EventSubscription(
            loop=asyncio.get_running_loop(),
            queue=deque(),
            ready=asyncio.Event(),
            lock=Lock(),
        )
        with self._lock:
            self._subscriptions.add(subscription)
        return subscription

    def unsubscribe(self, subscription: EventSubscription) -> None:
        with self._lock:
            self._subscriptions.discard(subscription)

    def publish(self, event_type: str, *, task_id: str | None = None, payload: Mapping[str, Any] | None = None) -> None:
        with self._lock:
            self._sequence += 1
            event = {
                "schema_version": 1,
                "event_id": str(uuid4()),
                "sequence": self._sequence,
                "type": event_type,
                "task_id": task_id,
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "payload": jsonable_encoder(dict(payload or {})),
            }
            subscriptions = tuple(self._subscriptions)
        for subscription in subscriptions:
            subscription.loop.call_soon_threadsafe(subscription.offer, event, self._queue_size)

    def heartbeat(self) -> dict[str, Any]:
        with self._lock:
            self._sequence += 1
            sequence = self._sequence
        return {
            "schema_version": 1,
            "event_id": str(uuid4()),
            "sequence": sequence,
            "type": "heartbeat",
            "task_id": None,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "payload": {},
        }