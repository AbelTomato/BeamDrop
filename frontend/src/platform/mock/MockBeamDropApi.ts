import type {
  AppSnapshot,
  BeamDropApi,
  BeamDropEvent,
  ReceiverSnapshot,
  SendRequest,
  StartReceiverRequest,
  TaskCreated,
} from '../BeamDropApi'

export class MockBeamDropApi implements BeamDropApi {
  private readonly listeners = new Set<(event: BeamDropEvent) => void>()
  private snapshot: AppSnapshot

  public constructor(snapshot: AppSnapshot) {
    this.snapshot = snapshot
  }

  public async getSnapshot(): Promise<AppSnapshot> { return this.snapshot }

  public async send(_request: SendRequest): Promise<TaskCreated> {
    return { taskId: 'mock-task', createdAt: this.snapshot.snapshotAt }
  }

  public async startReceiver(request: StartReceiverRequest): Promise<ReceiverSnapshot> {
    const receiver: ReceiverSnapshot = {
      state: 'running', host: request.host, port: request.port, saveDir: request.saveDir, lastError: null,
    }
    this.snapshot = { ...this.snapshot, receiver }
    return receiver
  }

  public async stopReceiver(): Promise<ReceiverSnapshot> {
    const receiver: ReceiverSnapshot = { state: 'stopped', host: null, port: null, saveDir: null, lastError: null }
    this.snapshot = { ...this.snapshot, receiver }
    return receiver
  }

  public subscribe(listener: (event: BeamDropEvent) => void): () => void {
    this.listeners.add(listener)
    return () => this.listeners.delete(listener)
  }

  public emit(event: BeamDropEvent): void {
    this.listeners.forEach((listener) => listener(event))
  }
}