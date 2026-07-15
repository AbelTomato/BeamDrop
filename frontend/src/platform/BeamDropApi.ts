/**
 * frontend domain port. Feature code must depend on this file rather
 * than REST, WebSocket, pybind, or Tauri-specific APIs.
 */

export const EVENT_SCHEMA_VERSION = 1 as const;

export type TaskState = "pending" | "running" | "canceling" | "canceled" | "completed" | "failed";
export type ReceiverState =
  | "stopped"
  | "starting"
  | "running"
  | "stopping"
  | "failed";
export type TransferDirection = "send" | "receive";
export type TransferStage =
  | "task_started"
  | "transferring"
  | "file_completed"
  | "task_completed";

export interface ErrorPayload {
  code: string;
  message: string;
  details?: Record<string, unknown>;
}

/** A path reference is deliberate: browsers must not upload a duplicate File for this flow. */
export interface SendRequest {
  sourcePath: string;
  targetHost: string;
  targetPort: number;
  chunkSize?: number;
}

export interface StartReceiverRequest {
  host: string;
  port: number;
  saveDir: string;
}

export interface TaskCreated {
  taskId: string;
  createdAt: string;
}

export interface TransferProgress {
  direction: TransferDirection;
  stage: TransferStage;
  fileIndex: number;
  fileCount: number;
  relativePath: string;
  currentFileBytes: number;
  currentFileTotalBytes: number;
  totalBytes: number | null;
  transferredBytes: number | null;
}

export interface TaskSnapshot {
  taskId: string;
  state: TaskState;
  request: SendRequest;
  createdAt: string;
  startedAt: string | null;
  finishedAt: string | null;
  progress: TransferProgress | null;
  error: ErrorPayload | null;
}

export interface ReceiverSnapshot {
  state: ReceiverState;
  host: string | null;
  port: number | null;
  saveDir: string | null;
  lastError: ErrorPayload | null;
}

export interface AppSnapshot {
  tasks: readonly TaskSnapshot[];
  receiver: ReceiverSnapshot;
  snapshotAt: string;
}

interface EventEnvelopeBase {
  schemaVersion: typeof EVENT_SCHEMA_VERSION;
  eventId: string;
  sequence: number;
  taskId: string | null;
  timestamp: string;
}

export interface TaskCreatedEvent extends EventEnvelopeBase {
  type: "task.created";
  taskId: string;
  payload: TaskSnapshot;
}

export interface TaskStartedEvent extends EventEnvelopeBase {
  type: "task.started";
  taskId: string;
  payload: TaskSnapshot;
}

export interface TransferProgressEvent extends EventEnvelopeBase {
  type: "transfer.progress";
  taskId: string;
  payload: TransferProgress;
}

export interface TaskCompletedEvent extends EventEnvelopeBase {
  type: "task.completed";
  taskId: string;
  payload: TaskSnapshot;
}

export interface TaskFailedEvent extends EventEnvelopeBase {
  type: "task.failed";
  taskId: string;
  payload: TaskSnapshot;
}

export interface TaskCanceledEvent extends EventEnvelopeBase {
  type: "task.canceled" | "task.canceling";
  taskId: string;
  payload: TaskSnapshot;
}

export interface ReceiverStartedEvent extends EventEnvelopeBase {
  type: "receiver.started";
  payload: ReceiverSnapshot;
}

export interface ReceiverStoppedEvent extends EventEnvelopeBase {
  type: "receiver.stopped";
  payload: ReceiverSnapshot;
}

export interface ErrorEvent extends EventEnvelopeBase {
  type: "error";
  payload: ErrorPayload;
}

export interface HeartbeatEvent extends EventEnvelopeBase {
  type: "heartbeat";
  payload: Record<string, never>;
}

export type KnownBeamDropEvent =
  | TaskCreatedEvent
  | TaskStartedEvent
  | TransferProgressEvent
  | TaskCompletedEvent
  | TaskFailedEvent
  | TaskCanceledEvent
  | ReceiverStartedEvent
  | ReceiverStoppedEvent
  | ErrorEvent
  | HeartbeatEvent;

/** Preserved for observability; consumers can ignore it without crashing. */
export interface UnknownBeamDropEvent extends EventEnvelopeBase {
  type: string;
  payload: unknown;
}

export type BeamDropEvent = KnownBeamDropEvent | UnknownBeamDropEvent;

const KNOWN_EVENT_TYPES: ReadonlySet<string> = new Set([
  "task.created",
  "task.started",
  "transfer.progress",
  "task.completed",
  "task.failed",
  "task.canceling",
  "task.canceled",
  "receiver.started",
  "receiver.stopped",
  "error",
  "heartbeat",
]);

export interface BeamDropApi {
  getSnapshot(): Promise<AppSnapshot>;
  send(request: SendRequest): Promise<TaskCreated>;
  cancel(taskId: string): Promise<TaskSnapshot>;
  startReceiver(request: StartReceiverRequest): Promise<ReceiverSnapshot>;
  stopReceiver(): Promise<ReceiverSnapshot>;
  subscribe(
    listener: (event: BeamDropEvent) => void,
    onSnapshot?: (snapshot: AppSnapshot) => void,
  ): () => void;
}

export function isKnownBeamDropEvent(
  event: BeamDropEvent,
): event is KnownBeamDropEvent {
  return KNOWN_EVENT_TYPES.has(event.type);
}
