import {
  EVENT_SCHEMA_VERSION,
  type AppSnapshot,
  type BeamDropApi,
  type BeamDropEvent,
  type ErrorPayload,
  type ReceiverSnapshot,
  type SendRequest,
  type StartReceiverRequest,
  type TaskCreated,
  type TaskSnapshot,
  type TransferProgress,
} from '../BeamDropApi'
import { BeamDropError, IncompatibleEventSchemaError } from '../errors'

type FetchLike = typeof fetch
type WebSocketFactory = (url: string) => WebSocket

export interface FastApiAdapterOptions {
  baseUrl: string
  fetch?: FetchLike
  webSocketFactory?: WebSocketFactory
  reconnectDelayMs?: number
  maxReconnectDelayMs?: number
}

/** The only frontend module allowed to know FastAPI JSON and WebSocket details. */
export class FastApiAdapter implements BeamDropApi {
  public readonly baseUrl: string
  private readonly request: FetchLike
  private readonly openWebSocket: WebSocketFactory
  private readonly reconnectDelayMs: number
  private readonly maxReconnectDelayMs: number

  public constructor(options: FastApiAdapterOptions | string) {
    const config = typeof options === 'string' ? { baseUrl: options } : options
    this.baseUrl = config.baseUrl.replace(/\/$/, '')
    // Browser fetch validates its receiver; storing it unbound then calling
    // this.request(...) makes `this` the adapter and throws Illegal invocation.
    this.request = config.fetch ?? globalThis.fetch.bind(globalThis)
    this.openWebSocket = config.webSocketFactory ?? ((url) => new WebSocket(url))
    this.reconnectDelayMs = config.reconnectDelayMs ?? 250
    this.maxReconnectDelayMs = config.maxReconnectDelayMs ?? 5_000
  }

  public async getSnapshot(): Promise<AppSnapshot> {
    return parseSnapshot(await this.getJson('/api/snapshot'))
  }

  public async send(request: SendRequest): Promise<TaskCreated> {
    return parseTaskCreated(await this.getJson('/api/transfers/send', 'POST', sendWire(request)))
  }

  public async cancel(taskId: string): Promise<TaskSnapshot> {
    return parseTask(await this.getJson(`/api/tasks/${encodeURIComponent(taskId)}/cancel`, 'POST'))
  }

  public async startReceiver(request: StartReceiverRequest): Promise<ReceiverSnapshot> {
    return parseReceiver(await this.getJson('/api/receiver/start', 'POST', receiverWire(request)))
  }

  public async stopReceiver(): Promise<ReceiverSnapshot> {
    return parseReceiver(await this.getJson('/api/receiver/stop', 'POST'))
  }

  public subscribe(
    listener: (event: BeamDropEvent) => void,
    onSnapshot?: (snapshot: AppSnapshot) => void,
  ): () => void {
    let disposed = false
    let socket: WebSocket | null = null
    let timer: number | null = null
    let retries = 0
    let sequence: number | null = null
    const connect = (): void => {
      if (disposed) return
      socket = this.openWebSocket(this.websocketUrl())
      socket.onopen = () => {
        retries = 0
        void this.refreshSnapshot(onSnapshot)
      }
      socket.onmessage = (message) => {
        try {
          const event = parseFastApiEventEnvelope(JSON.parse(String(message.data)))
          const gap = sequence !== null && event.sequence !== sequence + 1
          sequence = event.sequence
          if (gap) void this.refreshSnapshot(onSnapshot)
          listener(event)
        } catch (error) {
          if (error instanceof BeamDropError) listener(errorAsEvent(error))
        }
      }
      socket.onerror = () => socket?.close()
      socket.onclose = () => {
        if (disposed) return
        const delay = Math.min(this.reconnectDelayMs * 2 ** retries++, this.maxReconnectDelayMs)
        timer = window.setTimeout(connect, delay)
      }
    }
    connect()
    return () => {
      disposed = true
      if (timer !== null) window.clearTimeout(timer)
      socket?.close()
    }
  }

  private async getJson(path: string, method = 'GET', payload?: object): Promise<unknown> {
    let response: Response
    try {
      response = await this.request(`${this.baseUrl}${path}`, {
        method,
        headers: { accept: 'application/json', ...(payload ? { 'content-type': 'application/json' } : {}) },
        body: payload ? JSON.stringify(payload) : undefined,
      })
    } catch {
      throw new BeamDropError('BACKEND_UNAVAILABLE', 'BeamDrop backend is unavailable')
    }
    const body: unknown = await response.json().catch(() => null)
    if (!response.ok) throw apiError(body, response.status)
    return body
  }

  private websocketUrl(): string {
    const url = new URL(this.baseUrl)
    url.protocol = url.protocol === 'https:' ? 'wss:' : 'ws:'
    url.pathname = `${url.pathname.replace(/\/$/, '')}/ws/events`
    return url.toString()
  }

  private async refreshSnapshot(onSnapshot?: (snapshot: AppSnapshot) => void): Promise<void> {
    try {
      onSnapshot?.(await this.getSnapshot())
    } catch {
      // The existing snapshot remains usable until a future successful reconnect.
    }
  }
}

/** Validates envelope compatibility and maps its FastAPI payload to frontend types. */
export function parseFastApiEventEnvelope(value: unknown): BeamDropEvent {
  const wire = object(value, 'Event envelope')
  if (wire.schema_version !== EVENT_SCHEMA_VERSION) throw new IncompatibleEventSchemaError(wire.schema_version)
  const type = text(wire.type, 'type')
  const payload = object(wire.payload, 'event payload')
  const base = { schemaVersion: EVENT_SCHEMA_VERSION, eventId: text(wire.event_id, 'event_id'), sequence: numeric(wire.sequence, 'sequence'), type, taskId: nullableText(wire.task_id, 'task_id'), timestamp: text(wire.timestamp, 'timestamp') }
  switch (type) {
    case 'transfer.progress': return { ...base, type, payload: parseProgress(payload.progress) } as BeamDropEvent
    case 'task.created':
    case 'task.started':
    case 'task.completed':
    case 'task.failed':
    case 'task.canceling':
    case 'task.canceled': return { ...base, type, payload: parseTask(payload.task) } as BeamDropEvent
    case 'receiver.started':
    case 'receiver.stopped': return { ...base, type, payload: parseReceiver(payload.receiver) } as BeamDropEvent
    case 'error': return { ...base, type, payload: parseError(payload.error) } as BeamDropEvent
    default: return { ...base, payload: wire.payload } as BeamDropEvent
  }
}

function parseSnapshot(value: unknown): AppSnapshot {
  const wire = object(value, 'snapshot')
  if (!Array.isArray(wire.tasks)) throw invalid('tasks must be an array')
  return { tasks: wire.tasks.map(parseTask), receiver: parseReceiver(wire.receiver), snapshotAt: text(wire.snapshot_at, 'snapshot_at') }
}
function parseTaskCreated(value: unknown): TaskCreated { const wire = object(value, 'task response'); return { taskId: text(wire.task_id, 'task_id'), createdAt: text(wire.created_at, 'created_at') } }
function parseTask(value: unknown): TaskSnapshot { const w = object(value, 'task'); return { taskId: text(w.task_id, 'task_id'), state: text(w.state, 'state') as TaskSnapshot['state'], request: sendFromWire(w.request), createdAt: text(w.created_at, 'created_at'), startedAt: nullableText(w.started_at, 'started_at'), finishedAt: nullableText(w.finished_at, 'finished_at'), progress: w.progress === null ? null : parseProgress(w.progress), error: w.error === null ? null : parseError(w.error) } }
function parseReceiver(value: unknown): ReceiverSnapshot { const w = object(value, 'receiver'); return { state: text(w.state, 'state') as ReceiverSnapshot['state'], host: nullableText(w.host, 'host'), port: w.port === null ? null : numeric(w.port, 'port'), saveDir: nullableText(w.save_dir, 'save_dir'), lastError: w.last_error === null ? null : parseError(w.last_error) } }
function parseProgress(value: unknown): TransferProgress { const w = object(value, 'progress'); return { direction: text(w.direction, 'direction') as TransferProgress['direction'], stage: text(w.stage, 'stage') as TransferProgress['stage'], fileIndex: numeric(w.file_index, 'file_index'), fileCount: numeric(w.file_count, 'file_count'), relativePath: text(w.relative_path, 'relative_path'), currentFileBytes: numeric(w.current_file_bytes, 'current_file_bytes'), currentFileTotalBytes: numeric(w.current_file_total_bytes, 'current_file_total_bytes'), totalBytes: nullableNumber(w.total_bytes, 'total_bytes'), transferredBytes: nullableNumber(w.transferred_bytes, 'transferred_bytes') } }
function parseError(value: unknown): ErrorPayload { const w = object(value, 'error'); return { code: text(w.code, 'code'), message: text(w.message, 'message'), details: w.details == null ? undefined : object(w.details, 'details') } }
function sendFromWire(value: unknown): SendRequest { const w = object(value, 'request'); return { sourcePath: text(w.source_path, 'source_path'), targetHost: text(w.target_host, 'target_host'), targetPort: numeric(w.target_port, 'target_port'), chunkSize: w.chunk_size == null ? undefined : numeric(w.chunk_size, 'chunk_size') } }
function sendWire(value: SendRequest): object { return { source_path: value.sourcePath, target_host: value.targetHost, target_port: value.targetPort, chunk_size: value.chunkSize } }
function receiverWire(value: StartReceiverRequest): object { return { host: value.host, port: value.port, save_dir: value.saveDir } }
function apiError(value: unknown, status: number): BeamDropError { try { const error = parseError(object(value, 'response').error); return new BeamDropError(error.code, error.message, error.details) } catch { return new BeamDropError('HTTP_ERROR', `BeamDrop backend returned HTTP ${status}`) } }
function errorAsEvent(error: BeamDropError): BeamDropEvent { return { schemaVersion: EVENT_SCHEMA_VERSION, eventId: crypto.randomUUID(), sequence: -1, type: 'error', taskId: null, timestamp: new Date().toISOString(), payload: { code: error.code, message: error.message, details: error.details } } }
function object(value: unknown, name: string): Record<string, unknown> { if (typeof value !== 'object' || value === null || Array.isArray(value)) throw invalid(`${name} must be an object`); return value as Record<string, unknown> }
function text(value: unknown, name: string): string { if (typeof value !== 'string') throw invalid(`${name} must be a string`); return value }
function numeric(value: unknown, name: string): number { if (typeof value !== 'number' || !Number.isFinite(value)) throw invalid(`${name} must be a finite number`); return value }
function nullableText(value: unknown, name: string): string | null { return value === null ? null : text(value, name) }
function nullableNumber(value: unknown, name: string): number | null { return value === null ? null : numeric(value, name) }
function invalid(message: string): BeamDropError { return new BeamDropError('INVALID_BACKEND_RESPONSE', message) }