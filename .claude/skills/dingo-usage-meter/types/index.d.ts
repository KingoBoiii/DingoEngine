export type FiveHourReading = {
  /** 0 to 100: share of the 5-hour limit used. */
  percentUsed: number
  /** ISO 8601 time the window resets, when the API reported one. */
  resetsAt?: string
  /** When this reading was taken, epoch ms. */
  seenAt: number
}

declare module 'claude-code' {
  interface PluginState {
    'dingo-usage-meter': { reading: FiveHourReading | null; now: number }
  }
}
