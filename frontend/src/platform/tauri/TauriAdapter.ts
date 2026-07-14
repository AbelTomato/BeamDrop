import type {
  AppSnapshot,
  BeamDropApi,
  BeamDropEvent,
  ReceiverSnapshot,
  SendRequest,
  StartReceiverRequest,
  TaskCreated,
} from '../BeamDropApi'
import { BeamDropError } from '../errors'

/**
 * Tauri host reservation. It must implement the same port without introducing a
 * Tauri dependency into feature code or pretending native transfer exists today.
 */
export class TauriAdapter implements BeamDropApi {
  public getSnapshot(): Promise<AppSnapshot> { return Promise.reject(this.unsupported()) }
  public send(_request: SendRequest): Promise<TaskCreated> { return Promise.reject(this.unsupported()) }
  public startReceiver(_request: StartReceiverRequest): Promise<ReceiverSnapshot> {
    return Promise.reject(this.unsupported())
  }
  public stopReceiver(): Promise<ReceiverSnapshot> { return Promise.reject(this.unsupported()) }
  public subscribe(_listener: (event: BeamDropEvent) => void): () => void { throw this.unsupported() }

  private unsupported(): BeamDropError {
    return new BeamDropError('HOST_NOT_IMPLEMENTED', 'Tauri host is not implemented in this release')
  }
}