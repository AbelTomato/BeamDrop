import {
  EVENT_SCHEMA_VERSION,
  type AppSnapshot,
  type BeamDropApi,
  type BeamDropEvent,
  type ReceiverSnapshot,
  type SendRequest,
  type StartReceiverRequest,
  type TaskCreated,
} from '../BeamDropApi'
import { BeamDropError, IncompatibleEventSchemaError } from '../errors'

/**
 * FastAPI host boundary. Network transport is intentionally deferred to Slice 7;
 * this class reserves the only permitted location for fetch and WebSocket code.
 */
export class FastApiAdapter implements BeamDropApi {
  public readonly baseUrl: string

  public constructor(baseUrl: string) {
    this.baseUrl = baseUrl
  }

  public getSnapshot(): Promise<AppSnapshot> {
    return Promise.reject(this.notImplemented())
  }

  public send(_request: SendRequest): Promise<TaskCreated> {
    return Promise.reject(this.notImplemented())
  }

  public startReceiver(_request: StartReceiverRequest): Promise<ReceiverSnapshot> {
    return Promise.reject(this.notImplemented())
  }

  public stopReceiver(): Promise<ReceiverSnapshot> {
    return Promise.reject(this.notImplemented())
  }

  public subscribe(_listener: (event: BeamDropEvent) => void): () => void {
    throw this.notImplemented()
  }

  private notImplemented(): BeamDropError {
    return new BeamDropError('HOST_NOT_IMPLEMENTED', 'FastAPI transport will be implemented in Slice 7')
  }
}

/** Validates envelope compatibility before adapter consumers see an event. */
export function parseFastApiEventEnvelope(value: unknown): BeamDropEvent {
  if (!isRecord(value)) {
    throw new BeamDropError('INVALID_EVENT_ENVELOPE', 'Event envelope must be an object')
  }
  if (value.schema_version !== EVENT_SCHEMA_VERSION) {
    throw new IncompatibleEventSchemaError(value.schema_version)
  }
  if (typeof value.event_id !== 'string' || typeof value.sequence !== 'number' ||
      typeof value.type !== 'string' || typeof value.timestamp !== 'string') {
    throw new BeamDropError('INVALID_EVENT_ENVELOPE', 'Event envelope has invalid required fields')
  }
  if (value.task_id !== null && typeof value.task_id !== 'string') {
    throw new BeamDropError('INVALID_EVENT_ENVELOPE', 'Event taskId must be a string or null')
  }

  return {
    schemaVersion: EVENT_SCHEMA_VERSION,
    eventId: value.event_id,
    sequence: value.sequence,
    type: value.type,
    taskId: value.task_id,
    timestamp: value.timestamp,
    payload: value.payload,
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}