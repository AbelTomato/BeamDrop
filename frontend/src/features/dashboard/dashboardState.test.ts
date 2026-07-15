import assert from 'node:assert/strict'
import test from 'node:test'
import { applyEvent, applySnapshot, initialDashboardState } from './dashboardState'

const snapshot = { snapshotAt: '2026-07-15T00:00:00Z', receiver: { state: 'stopped' as const, host: null, port: null, saveDir: null, lastError: null }, tasks: [{ taskId: 'task-1', state: 'running' as const, request: { sourcePath: 'D:/data.bin', targetHost: '127.0.0.1', targetPort: 9090 }, createdAt: '2026-07-15T00:00:00Z', startedAt: null, finishedAt: null, progress: null, error: null }] }

test('snapshot is authoritative and a progress event only updates its matching task', () => {
  const state = applySnapshot(initialDashboardState, snapshot)
  const next = applyEvent(state, { schemaVersion: 1, eventId: 'event-1', sequence: 1, type: 'transfer.progress', taskId: 'task-1', timestamp: '2026-07-15T00:00:01Z', payload: { direction: 'send', stage: 'transferring', fileIndex: 0, fileCount: 1, relativePath: 'data.bin', currentFileBytes: 30, currentFileTotalBytes: 100, totalBytes: 100, transferredBytes: 30 } })
  assert.equal(next.connection, 'online')
  assert.equal(next.snapshot?.tasks[0].progress?.transferredBytes, 30)
})

test('unknown events are ignored and backend unavailable preserves the last snapshot', () => {
  const state = applySnapshot(initialDashboardState, snapshot)
  const unchanged = applyEvent(state, { schemaVersion: 1, eventId: 'event-unknown', sequence: 2, type: 'future.event', taskId: null, timestamp: '2026-07-15T00:00:02Z', payload: { future: true } })
  assert.equal(unchanged, state)
  const offline = applyEvent(state, { schemaVersion: 1, eventId: 'event-error', sequence: -1, type: 'error', taskId: null, timestamp: '2026-07-15T00:00:03Z', payload: { code: 'BACKEND_UNAVAILABLE', message: 'offline' } })
  assert.equal(offline.connection, 'offline')
  assert.equal(offline.snapshot?.tasks.length, 1)
})