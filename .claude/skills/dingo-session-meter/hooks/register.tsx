import { atom, read, update } from 'claude-code'
import type { EngineInterface, Register, SessionContextUsage, SessionRateLimit } from 'claude-code'

import type { CacheReading, ContextReading, FiveHourReading } from '../types'

const context = atom({ plugin: 'dingo-session-meter', key: 'context' } as const, null)
const cache = atom({ plugin: 'dingo-session-meter', key: 'cache' } as const, null)
const fiveHour = atom({ plugin: 'dingo-session-meter', key: 'fiveHour' } as const, null)
const now = atom({ plugin: 'dingo-session-meter', key: 'now' } as const, 0)

// Shared across sessions, so every open session shows the newest 5h reading.
const STORE_KEY = 'fiveHour'
const TICK_MS = 15_000
// Both rows draw label and bar in fixed cells, so they line up in any font.
const LABEL_CELLS = 9
const BAR_TRACK = '#3a3a3a'
const HOUR_MS = 60 * 60_000
const FIVE_MIN_MS = 5 * 60_000

function toReading(c: SessionContextUsage): ContextReading {
  return { tokens: c.tokens, window: c.window, percent: c.percent }
}

function pickFiveHour(limits: SessionRateLimit[], seenAt: number): FiveHourReading | null {
  const window = limits.find(l => l.kind === 'five_hour')

  return window ? { percentUsed: window.percentUsed, resetsAt: window.resetsAt, seenAt } : null
}

function isFiveHourReading(value: unknown): value is FiveHourReading {
  const v = value as FiveHourReading | null

  return typeof v === 'object' && v !== null && typeof v.percentUsed === 'number' && typeof v.seenAt === 'number'
}

async function saveFiveHour($: EngineInterface, fresh: FiveHourReading) {
  await update($, fiveHour, () => fresh)
  await $.store.set(STORE_KEY, fresh)
}

async function syncFromStore($: EngineInterface) {
  const stored = await $.store.get(STORE_KEY)
  if (!isFiveHourReading(stored)) {
    return
  }

  const current = await read($, fiveHour)
  if (current === null || stored.seenAt > current.seenAt) {
    await update($, fiveHour, () => stored)
  }
}

async function tick($: EngineInterface) {
  try {
    await syncFromStore($)
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

    await syncFromStore($)
    const usage = await $.session.usage()
    await update($, context, () => toReading(usage.context))
    const fresh = pickFiveHour(usage.rateLimits, await $.clock.now())
    if (fresh) {
      await saveFiveHour($, fresh)
    }

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
    if (e.changed.includes('rateLimits')) {
      const fresh = pickFiveHour(e.rateLimits, await $.clock.now())
      if (fresh) {
        await saveFiveHour($, fresh)
      }
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
    const r = await read($, fiveHour)
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

    // 5-hour limit
    let fiveHourRow
    if (r === null) {
      fiveHourRow = (
        <Box>
          {label('5h limit')}
          {bar(0, 'gray')}
          <Text dimColor> no reading yet · shows after your next message</Text>
        </Box>
      )
    } else {
      const resetAt = r.resetsAt ? Date.parse(r.resetsAt) : NaN
      const hasReset = Number.isFinite(resetAt) && resetAt <= t
      const used = hasReset ? 0 : r.percentUsed
      const left = Math.max(0, Math.round((100 - used) * 10) / 10)
      const resetText = hasReset
        ? 'window reset · refreshes after next message'
        : Number.isFinite(resetAt)
          ? `resets in ${formatDuration(resetAt - t)}`
          : 'reset time unknown'
      const isStale = !hasReset && t - r.seenAt >= 5 * 60_000
      const ageText = isStale ? ` · as of ${formatDuration(t - r.seenAt)} ago` : ''

      fiveHourRow = (
        <Box>
          {label('5h limit')}
          {bar(used, levelColor(used))}
          <Text color={levelColor(used)} bold>
            {` ${used}% used`}
          </Text>
          <Text dimColor>{` · ${left}% left · ${resetText}${ageText}`}</Text>
        </Box>
      )
    }

    // Context window, and whether the prompt cache is live
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

    const contextRow = (
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

    const mine = (
      <Box flexDirection="column">
        {fiveHourRow}
        {contextRow}
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
