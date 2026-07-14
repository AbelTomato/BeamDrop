import type { BeamDropApi } from './BeamDropApi'
import { FastApiAdapter } from './fastapi/FastApiAdapter'

/** Slice 3 composition point; later desktop builds can select TauriAdapter here. */
export function createBeamDropApi(baseUrl: string): BeamDropApi {
  return new FastApiAdapter(baseUrl)
}