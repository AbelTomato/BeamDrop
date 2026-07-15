import type {
  AppSnapshot,
  BeamDropApi,
  BeamDropEvent,
  ReceiverSnapshot,
  SendRequest,
  StartReceiverRequest,
  TaskCreated,
  TaskSnapshot,
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

  public async cancel(taskId: string): Promise<TaskSnapshot> {
    const task = this.snapshot.tasks.find((item) => item.taskId === taskId)
    if (!task) throw new Error(`unknown mock task '${taskId}'`)
    const canceled: TaskSnapshot = { ...task, state: 'canceled', finishedAt: new Date().toISOString(), error: null }
    this.snapshot = { ...this.snapshot, tasks: this.snapshot.tasks.map((item) => item.taskId === taskId ? canceled : item) }
    return canceled
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

  public subscribe(
    listener: (event: BeamDropEvent) => void,
    onSnapshot?: (snapshot: AppSnapshot) => void,
  ): () => void {
    this.listeners.add(listener)
    onSnapshot?.(this.snapshot)
    return () => this.listeners.delete(listener)
  }

  public emit(event: BeamDropEvent): void {
    this.listeners.forEach((listener) => listener(event))
  }
}