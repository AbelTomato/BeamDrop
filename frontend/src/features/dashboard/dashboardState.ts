import { isKnownBeamDropEvent, type AppSnapshot, type BeamDropEvent, type ErrorPayload, type ReceiverSnapshot, type TaskSnapshot, type TransferProgress } from '../../platform/BeamDropApi'

export interface DashboardState {
  snapshot: AppSnapshot | null
  connection: 'connecting' | 'online' | 'offline'
  lastError: ErrorPayload | null
}

export const initialDashboardState: DashboardState = { snapshot: null, connection: 'connecting', lastError: null }

export function applySnapshot(state: DashboardState, snapshot: AppSnapshot): DashboardState {
  return { ...state, snapshot, connection: 'online' }
}

export function applyEvent(state: DashboardState, event: BeamDropEvent): DashboardState {
  if (!isKnownBeamDropEvent(event)) return state
  if (event.type === 'error') return { ...state, connection: event.payload.code === 'BACKEND_UNAVAILABLE' ? 'offline' : state.connection, lastError: event.payload }
  if (!state.snapshot) return state
  if (event.type === 'receiver.started' || event.type === 'receiver.stopped') return withReceiver(state, event.payload)
  if (event.type === 'task.created' || event.type === 'task.started' || event.type === 'task.completed' || event.type === 'task.failed' || event.type === 'task.canceling' || event.type === 'task.canceled') return withTask(state, event.payload)
  if (event.type === 'transfer.progress' && event.taskId !== null) return withProgress(state, event.taskId, event.payload)
  return state
}

function withReceiver(state: DashboardState, receiver: ReceiverSnapshot): DashboardState { return { ...state, snapshot: { ...state.snapshot!, receiver } } }
function withTask(state: DashboardState, task: TaskSnapshot): DashboardState {
  const tasks = state.snapshot!.tasks.filter((item) => item.taskId !== task.taskId)
  return { ...state, snapshot: { ...state.snapshot!, tasks: [...tasks, task] } }
}
function withProgress(state: DashboardState, taskId: string, progress: TransferProgress): DashboardState {
  return { ...state, snapshot: { ...state.snapshot!, tasks: state.snapshot!.tasks.map((task) => task.taskId === taskId ? { ...task, progress } : task) } }
}