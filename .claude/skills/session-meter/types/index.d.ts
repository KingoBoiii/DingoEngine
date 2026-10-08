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

declare module 'claude-code' {
  interface PluginState {
    'session-meter': {
      context: ContextReading | null
      cache: CacheReading | null
      now: number
    }
  }
}
