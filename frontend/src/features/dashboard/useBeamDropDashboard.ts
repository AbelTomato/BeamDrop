import { useCallback, useEffect, useReducer, useState } from 'react'
import type { BeamDropApi, BeamDropEvent, ErrorPayload, SendRequest, StartReceiverRequest } from '../../platform/BeamDropApi'
import { BeamDropError } from '../../platform/errors'
import { applyEvent, applySnapshot, initialDashboardState } from './dashboardState'

type Action = { type: 'snapshot'; value: Awaited<ReturnType<BeamDropApi['getSnapshot']>> } | { type: 'event'; value: BeamDropEvent }
function reducer(state: typeof initialDashboardState, action: Action) { return action.type === 'snapshot' ? applySnapshot(state, action.value) : applyEvent(state, action.value) }
function asPayload(error: unknown): ErrorPayload { return error instanceof BeamDropError ? { code: error.code, message: error.message, details: error.details } : { code: 'UNEXPECTED_ERROR', message: 'Unexpected application error' } }
function errorEvent(payload: ErrorPayload): BeamDropEvent { return { schemaVersion: 1, eventId: crypto.randomUUID(), sequence: -1, type: 'error', taskId: null, timestamp: new Date().toISOString(), payload } }

export function useBeamDropDashboard(api: BeamDropApi) {
  const [state, dispatch] = useReducer(reducer, initialDashboardState)
  const [pendingAction, setPendingAction] = useState<'send' | 'receiver' | null>(null)

  useEffect(() => {
    let active = true
    void api.getSnapshot().then((snapshot) => active && dispatch({ type: 'snapshot', value: snapshot })).catch((error: unknown) => active && dispatch({ type: 'event', value: errorEvent(asPayload(error)) }))
    return api.subscribe((event) => dispatch({ type: 'event', value: event }), (snapshot) => dispatch({ type: 'snapshot', value: snapshot }))
  }, [api])

  const send = useCallback(async (request: SendRequest) => { setPendingAction('send'); try { await api.send(request) } catch (error) { dispatch({ type: 'event', value: errorEvent(asPayload(error)) }) } finally { setPendingAction(null) } }, [api])
  const cancel = useCallback(async (taskId: string) => { try { const task = await api.cancel(taskId); dispatch({ type: 'event', value: { schemaVersion: 1, eventId: crypto.randomUUID(), sequence: -1, type: task.state === 'canceled' ? 'task.canceled' : 'task.canceling', taskId, timestamp: new Date().toISOString(), payload: task } }) } catch (error) { dispatch({ type: 'event', value: errorEvent(asPayload(error)) }) } }, [api])
  const startReceiver = useCallback(async (request: StartReceiverRequest) => { setPendingAction('receiver'); try { const receiver = await api.startReceiver(request); dispatch({ type: 'event', value: { schemaVersion: 1, eventId: crypto.randomUUID(), sequence: -1, type: 'receiver.started', taskId: null, timestamp: new Date().toISOString(), payload: receiver } }) } catch (error) { dispatch({ type: 'event', value: errorEvent(asPayload(error)) }) } finally { setPendingAction(null) } }, [api])
  const stopReceiver = useCallback(async () => { setPendingAction('receiver'); try { const receiver = await api.stopReceiver(); dispatch({ type: 'event', value: { schemaVersion: 1, eventId: crypto.randomUUID(), sequence: -1, type: 'receiver.stopped', taskId: null, timestamp: new Date().toISOString(), payload: receiver } }) } catch (error) { dispatch({ type: 'event', value: errorEvent(asPayload(error)) }) } finally { setPendingAction(null) } }, [api])
  return { state, pendingAction, send, cancel, startReceiver, stopReceiver }
}