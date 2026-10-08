import { atom, read, update } from 'claude-code'
import type { EngineInterface, Register, SessionContextUsage } from 'claude-code'

import type { CacheReading, ContextReading } from '../types'

const context = atom({ plugin: 'session-meter', key: 'context' } as const, null)
const cache = atom({ plugin: 'session-meter', key: 'cache' } as const, null)
const now = atom({ plugin: 'session-meter', key: 'now' } as const, 0)

const TICK_MS = 15_000
// Label and bar in fixed cells, shared with usage-meter, so the bars line up in any font.
const LABEL_CELLS = 9
const BAR_TRACK = '#3a3a3a'
const HOUR_MS = 60 * 60_000
const FIVE_MIN_MS = 5 * 60_000

function toReading(c: SessionContextUsage): ContextReading {
  return { tokens: c.tokens, window: c.window, percent: c.percent }
}

async function tick($: EngineInterface) {
  try {
    const t = await $.clock.now()
    await update($, now, () => t)
  } catch {
    // A missed tick only delays the countdowns by one period.
  }
}

function formatTokens(n: number): string {
  if (n >= 1_000_000) {
    return `${(n / 1_000_000).toFixed(n >= 10_000_000 ? 0 : 1)}M`
  }

  return n >= 1_000 ? `${Math.round(n / 1_000)}k` : `${n}`
}

function formatDuration(ms: number): string {
  const totalSeconds = Math.max(0, Math.ceil(ms / 1_000))
  const totalMinutes = Math.floor(totalSeconds / 60)
  const hours = Math.floor(totalMinutes / 60)

  if (hours > 0) {
    return `${hours}h ${totalMinutes % 60}m`
  }

  return totalMinutes >= 1 ? `${totalMinutes}m` : `${totalSeconds}s`
}

function levelColor(percent: number): string {
  return percent >= 80 ? 'red' : percent >= 50 ? 'yellow' : 'green'
}

export const register: Register = (on, options) => {
  const ttlMs = options.cacheTtl === '5m' ? FIVE_MIN_MS : HOUR_MS

  on('session.start', async ($, e, next) => {
    const started = await next(e)

    const usage = await $.session.usage()
    await update($, context, () => toReading(usage.context))

    await tick($)
    $.clock.every(TICK_MS, () => void tick($))

    return started
  })

  // A resumed session (a restart included) picks up the cache where the transcript left it.
  on('classic.SessionStart', async ($, e, next) => {
    const seconds = e.seconds_since_last_response
    if ((e.source === 'resume' || e.source === 'fork') && seconds !== undefined) {
      const reading: CacheReading = {
        lastAt: (await $.clock.now()) - seconds * 1_000,
        readTokens: e.context_tokens ?? 0,
        writeTokens: 0,
        inputTokens: 0,
      }
      const current = await read($, cache)
      if (current === null || current.lastAt < reading.lastAt) {
        await update($, cache, () => reading)
      }
    }

    return next(e)
  })

  // A /clear starts a new conversation: the old cache entry no longer applies.
  on('session.end', async ($, e, next) => {
    if (e.reason === 'clear') {
      await update($, cache, () => null)
    }

    return next(e)
  })

  on('session.measure', async ($, e, next) => {
    if (e.changed.includes('context')) {
      await update($, context, () => toReading(e.context))
    }

    return next(e)
  })

  // Every main-thread API response restarts the cache's TTL.
  on('turn.step', async function* ($, e, next) {
    const result = yield* next(e)

    if (!e.agentId && result.usage) {
      const u = result.usage
      const reading: CacheReading = {
        lastAt: await $.clock.now(),
        readTokens: u.cache_read_input_tokens,
        writeTokens: u.cache_creation_input_tokens,
        inputTokens: u.input_tokens,
      }
      await update($, cache, () => reading)
      await update($, now, () => reading.lastAt)
    }

    return result
  })

  on('ui.render', { component: 'AbovePrompt' }, async ($, e, next) => {
    const below = await next(e)
    if (e.props.hasSurvey) {
      return below
    }

    const ctx = await read($, context)
    const c = await read($, cache)
    await read($, now) // subscribes the band to the countdown ticker
    const t = await $.clock.now()
    const { Box, Text } = $.ui.resolve(e)

    const width = Math.max(10, Math.min(24, e.props.bodyColumns - 70))

    const label = (text: string) => (
      <Box width={LABEL_CELLS} flexShrink={0}>
        <Text bold>{text}</Text>
      </Box>
    )

    // Solid cells, not glyphs: a proportional font draws █ and ░ wider than a cell.
    const bar = (percent: number, color: string) => {
      const filled = Math.round((Math.max(0, Math.min(percent, 100)) / 100) * width)
      const parts = []
      if (filled > 0) {
        parts.push(
          <Box width={filled} flexShrink={0} backgroundColor={color}>
            <Text>{' '.repeat(filled)}</Text>
          </Box>,
        )
      }
      if (filled < width) {
        parts.push(
          <Box width={width - filled} flexShrink={0} backgroundColor={BAR_TRACK}>
            <Text>{' '.repeat(width - filled)}</Text>
          </Box>,
        )
      }

      return (
        <Box width={width} flexShrink={0} overflow="hidden">
          {parts}
        </Box>
      )
    }

    const pct = ctx?.percent
    const ctxColor = pct === undefined ? 'gray' : levelColor(pct)
    const ctxText =
      ctx === null || pct === undefined
        ? ' no reading yet'
        : ` ${pct}% · ${formatTokens(ctx.tokens ?? 0)} / ${formatTokens(ctx.window)}`

    const leftMs = c === null ? 0 : c.lastAt + ttlMs - t
    const isLive = leftMs > 0
    const cacheColor = !isLive ? 'red' : leftMs > 5 * 60_000 ? 'green' : 'yellow'
    const cacheText = isLive
      ? ` · cold in ${formatDuration(leftMs)}`
      : c === null
        ? ''
        : ` · re-caches ~${formatTokens(c.readTokens + c.writeTokens)} next message`

    const mine = (
      <Box>
        {label('context')}
        {bar(pct ?? 0, ctxColor)}
        <Text color={ctxColor} bold>
          {ctxText}
        </Text>
        <Text dimColor> │ </Text>
        <Text bold>cache </Text>
        <Text color={cacheColor} bold>
          {isLive ? '● live' : '○ cold'}
        </Text>
        <Text dimColor>{cacheText}</Text>
      </Box>
    )

    // Stack above any band another plugin drew beneath this one.
    return below.type === 'engine' ? mine : (
      <Box flexDirection="column">
        {mine}
        {below}
      </Box>
    )
  })
}
