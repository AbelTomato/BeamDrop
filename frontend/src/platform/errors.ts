export class BeamDropError extends Error {
  public readonly code: string
  public readonly details: Record<string, unknown> | undefined

  public constructor(
    code: string,
    message: string,
    details?: Record<string, unknown>,
  ) {
    super(message)
    this.code = code
    this.details = details
    this.name = 'BeamDropError'
  }
}

export class IncompatibleEventSchemaError extends BeamDropError {
  public constructor(actualVersion: unknown) {
    super(
      'INCOMPATIBLE_EVENT_SCHEMA',
      `Unsupported BeamDrop event schema version: ${String(actualVersion)}`,
      { actualVersion },
    )
    this.name = 'IncompatibleEventSchemaError'
  }
}