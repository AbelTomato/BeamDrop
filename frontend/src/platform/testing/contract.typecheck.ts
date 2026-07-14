import { EVENT_SCHEMA_VERSION, isKnownBeamDropEvent, type AppSnapshot } from '../BeamDropApi'
import { parseFastApiEventEnvelope } from '../fastapi/FastApiAdapter'
import { MockBeamDropApi } from '../mock/MockBeamDropApi'

const snapshot: AppSnapshot = {
  snapshotAt: '2026-07-14T17:00:00+08:00',
  tasks: [],
  receiver: { state: 'stopped', host: null, port: null, saveDir: null, lastError: null },
}

const mock: MockBeamDropApi = new MockBeamDropApi(snapshot)
void mock.startReceiver({ host: '127.0.0.1', port: 9090, saveDir: 'received' })

const unknownEvent = parseFastApiEventEnvelope({
  schema_version: EVENT_SCHEMA_VERSION,
  event_id: 'event-1', sequence: 1, type: 'future.event', task_id: null,
  timestamp: '2026-07-14T17:00:00+08:00', payload: {},
})
if (isKnownBeamDropEvent(unknownEvent)) {
  throw new Error('Future event must not be treated as a known event')
}