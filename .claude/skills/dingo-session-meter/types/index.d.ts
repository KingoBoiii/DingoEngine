export type ContextReading = {
  /** Input tokens the last response was answered over; absent before the first response. */
  tokens?: number
  /** The model's context window, in tokens. */
  window: number
  /** `tokens` over `window`, 0 to 100; absent before the first response. */
  percent?: number
}

export type CacheReading = {
  /** When the last main-thread response arrived, epoch ms. */
  lastAt: number
  /** Input tokens that response read from the prompt cache. */
  readTokens: number
  /** Input tokens that response wrote to the prompt cache. */
  writeTokens: number
  /** Uncached input tokens of that response. */
  inputTokens: number
}

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
    'dingo-session-meter': {
      context: ContextReading | null
      cache: CacheReading | null
      fiveHour: FiveHourReading | null
      now: number
    }
  }
}
