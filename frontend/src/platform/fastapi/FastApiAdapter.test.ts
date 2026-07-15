import assert from 'node:assert/strict'
import test from 'node:test'

import { FastApiAdapter, parseFastApiEventEnvelope } from './FastApiAdapter'
import { IncompatibleEventSchemaError } from '../errors'
import type { ReceiverStartedEvent, TransferProgress } from '../BeamDropApi'

const snapshot = {
  tasks: [],
  receiver: { state: 'stopped', host: null, port: null, save_dir: null, last_error: null },
  snapshot_at: '2026-07-15T12:00:00Z',
}

function response(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), { status, headers: { 'content-type': 'application/json' } })
}

function nextTurn(): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, 0))
}

class FakeSocket {
  public onopen: (() => void) | null = null
  public onclose: (() => void) | null = null
  public onerror: (() => void) | null = null
  public onmessage: ((event: MessageEvent) => void) | null = null
  public closed = false
  public close(): void { this.closed = true; this.onclose?.() }
  public open(): void { this.onopen?.() }
  public message(body: unknown): void { this.onmessage?.({ data: JSON.stringify(body) } as MessageEvent) }
}

test('maps REST snake_case payloads and requests', async () => {
  const calls: RequestInfo[] = []
  const request = async (url: RequestInfo): Promise<Response> => {
    calls.push(url)
    return calls.length === 1 ? response(snapshot) : response({ task_id: 'task-1', created_at: '2026-07-15T12:00:00Z' })
  }
  const adapter = new FastApiAdapter({ baseUrl: 'http://127.0.0.1:8000/', fetch: request as typeof fetch })
  assert.equal((await adapter.getSnapshot()).receiver.saveDir, null)
  assert.deepEqual(await adapter.send({ sourcePath: 'data.bin', targetHost: '127.0.0.1', targetPort: 9000 }), { taskId: 'task-1', createdAt: '2026-07-15T12:00:00Z' })
  assert.equal(calls[1], 'http://127.0.0.1:8000/api/transfers/send')
})

test('normalizes unavailable and structured HTTP errors', async () => {
  const offline = new FastApiAdapter({ baseUrl: 'http://localhost:8000', fetch: (() => Promise.reject(new TypeError('failed'))) as typeof fetch })
  await assert.rejects(offline.getSnapshot(), { code: 'BACKEND_UNAVAILABLE' })
  const rejected = new FastApiAdapter({ baseUrl: 'http://localhost:8000', fetch: (() => Promise.resolve(response({ error: { code: 'TASK_NOT_FOUND', message: 'missing', details: null } }, 404))) as typeof fetch })
  await assert.rejects(rejected.getSnapshot(), { code: 'TASK_NOT_FOUND' })
})

test('binds the default browser fetch to its global receiver', async () => {
  const originalFetch = globalThis.fetch
  let called = false
  globalThis.fetch = function (this: unknown): Promise<Response> {
    assert.equal(this, globalThis)
    called = true
    return Promise.resolve(response(snapshot))
  } as typeof fetch
  try {
    const adapter = new FastApiAdapter('http://127.0.0.1:8000')
    await adapter.getSnapshot()
    assert.equal(called, true)
  } finally {
    globalThis.fetch = originalFetch
  }
})

test('rejects incompatible envelopes before exposing them', () => {
  assert.throws(() => parseFastApiEventEnvelope({ schema_version: 2 }), IncompatibleEventSchemaError)
})

test('accepts initial progress without total byte counts', () => {
  const event = parseFastApiEventEnvelope({
    schema_version: 1, event_id: 'progress-1', sequence: 1, type: 'transfer.progress', task_id: 'task-1', timestamp: '2026-07-15T12:00:00Z',
    payload: { progress: { direction: 'send', stage: 'task_started', file_index: 1, file_count: 1, relative_path: 'data.bin', current_file_bytes: 0, current_file_total_bytes: 0, total_bytes: null, transferred_bytes: null } },
  })
  assert.equal(event.type, 'transfer.progress')
  assert.equal((event.payload as TransferProgress).totalBytes, null)
})

test('accepts the flattened receiver DTO from receiver.started events', () => {
  const event = parseFastApiEventEnvelope({
    schema_version: 1, event_id: 'receiver-1', sequence: 1, type: 'receiver.started', task_id: null, timestamp: '2026-07-15T12:00:00Z',
    payload: { receiver: { state: 'running', host: '127.0.0.1', port: 9090, save_dir: 'D:/received', last_error: null } },
  })
  assert.equal(event.type, 'receiver.started')
  const receiverEvent = event as ReceiverStartedEvent
  assert.equal(receiverEvent.payload.host, '127.0.0.1')
  assert.equal(receiverEvent.payload.saveDir, 'D:/received')
})

test('preserves unknown task-like events instead of applying a task DTO parser', () => {
  const event = parseFastApiEventEnvelope({
    schema_version: 1, event_id: 'future-task-event', sequence: 1, type: 'task.paused', task_id: 'task-1', timestamp: '2026-07-15T12:00:00Z',
    payload: { future_value: true },
  })
  assert.equal(event.type, 'task.paused')
  assert.deepEqual(event.payload, { future_value: true })
})

test('refreshes snapshots on connect and sequence gaps, then disposes its socket', async () => {
  const sockets: FakeSocket[] = []
  const adapter = new FastApiAdapter({
    baseUrl: 'http://localhost:8000',
    fetch: (() => Promise.resolve(response(snapshot))) as typeof fetch,
    webSocketFactory: () => { const socket = new FakeSocket(); sockets.push(socket); return socket as unknown as WebSocket },
  })
  const snapshots: unknown[] = []
  const unsubscribe = adapter.subscribe(() => undefined, (value) => snapshots.push(value))
  sockets[0]?.open()
  await nextTurn()
  sockets[0]?.message({ schema_version: 1, event_id: 'one', sequence: 1, type: 'heartbeat', task_id: null, timestamp: '2026-07-15T12:00:00Z', payload: {} })
  sockets[0]?.message({ schema_version: 1, event_id: 'three', sequence: 3, type: 'heartbeat', task_id: null, timestamp: '2026-07-15T12:00:01Z', payload: {} })
  await nextTurn()
  assert.equal(snapshots.length, 2)
  unsubscribe()
  assert.equal(sockets[0]?.closed, true)
})