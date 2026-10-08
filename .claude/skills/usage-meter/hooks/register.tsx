import { atom, read, update } from 'claude-code'
import type { EngineInterface, Register, SessionRateLimit } from 'claude-code'

import type { FiveHourReading } from '../types'

const reading = atom({ plugin: 'usage-meter', key: 'reading' } as const, null)
const now = atom({ plugin: 'usage-meter', key: 'now' } as const, 0)

// Shared across sessions, so every open session shows the newest reading.
const STORE_KEY = 'fiveHour'
const TICK_MS = 30_000
// Label column width, shared with session-meter so the two bars line up.
const LABEL_CELLS = 9
const BAR_TRACK = '#3a3a3a'

function pickFiveHour(limits: SessionRateLimit[], seenAt: number): FiveHourReading | null {
  const window = limits.find(l => l.kind === 'five_hour')

  return window ? { percentUsed: window.percentUsed, resetsAt: window.resetsAt, seenAt } : null
}

function isReading(value: unknown): value is FiveHourReading {
  const v = value as FiveHourReading | null

  return typeof v === 'object' && v !== null && typeof v.percentUsed === 'number' && typeof v.seenAt === 'number'
}

async function save($: EngineInterface, fresh: FiveHourReading) {
  await update($, reading, () => fresh)
  await $.store.set(STORE_KEY, fresh)
}

async function syncFromStore($: EngineInterface) {
  const stored = await $.store.get(STORE_KEY)
  if (!isReading(stored)) {
    return
  }

  const current = await read($, reading)
  if (current === null || stored.seenAt > current.seenAt) {
    await update($, reading, () => stored)
  }
}

async function tick($: EngineInterface) {
  try {
    await syncFromStore($)
    const t = await $.clock.now()
    await update($, now, () => t)
  } catch {
    // A missed tick only delays the countdown by one period.
  }
}

function formatDuration(ms: number): string {
  const totalMinutes = Math.max(0, Math.ceil(ms / 60_000))
  const hours = Math.floor(totalMinutes / 60)
  const minutes = totalMinutes % 60

  return hours > 0 ? `${hours}h ${minutes}m` : `${minutes}m`
}

export const register: Register = on => {
  on('session.start', async ($, e, next) => {
    const started = await next(e)

    await syncFromStore($)
    const fresh = pickFiveHour((await $.session.usage()).rateLimits, await $.clock.now())
    if (fresh) {
      await save($, fresh)
    }

    $.clock.every(TICK_MS, () => void tick($))

    return started
  })

  on('session.measure', async ($, e, next) => {
    if (e.changed.includes('rateLimits')) {
      const fresh = pickFiveHour(e.rateLimits, await $.clock.now())
      if (fresh) {
        await save($, fresh)
      }
    }

    return next(e)
  })

  on('ui.render', { component: 'AbovePrompt' }, async ($, e, next) => {
    const below = await next(e)
    if (e.props.hasSurvey) {
      return below
    }

    const { Box } = $.ui.resolve(e)
    const mine = await (async () => {
      const r = await read($, reading)
      await read($, now) // subscribes the band to the countdown ticker
      const t = await $.clock.now()
      const { Text } = $.ui.resolve(e)

      if (r === null) {
        return (
          <Box>
            <Box width={LABEL_CELLS} flexShrink={0}>
              <Text bold>5h limit</Text>
            </Box>
            <Text dimColor>no reading yet · shows after your next message</Text>
          </Box>
        )
      }

      const resetAt = r.resetsAt ? Date.parse(r.resetsAt) : NaN
      const hasReset = Number.isFinite(resetAt) && resetAt <= t
      const used = hasReset ? 0 : r.percentUsed
      const left = Math.max(0, Math.round((100 - used) * 10) / 10)

      const width = Math.max(10, Math.min(24, e.props.bodyColumns - 70))
      const filled = Math.round((Math.min(used, 100) / 100) * width)
      const color = used >= 80 ? 'red' : used >= 50 ? 'yellow' : 'green'

      // Solid cells, not glyphs: a proportional font draws █ and ░ wider than a cell.
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
      const bar = (
        <Box width={width} flexShrink={0} overflow="hidden">
          {parts}
        </Box>
      )

      const resetText = hasReset
        ? 'window reset · refreshes after next message'
        : Number.isFinite(resetAt)
          ? `resets in ${formatDuration(resetAt - t)}`
          : 'reset time unknown'
      const isStale = !hasReset && t - r.seenAt >= 5 * 60_000
      const ageText = isStale ? ` · as of ${formatDuration(t - r.seenAt)} ago` : ''

      return (
        <Box>
          <Box width={LABEL_CELLS} flexShrink={0}>
            <Text bold>5h limit</Text>
          </Box>
          {bar}
          <Text color={color} bold>
            {' '}
            {used}% used
          </Text>
          <Text dimColor>
            {' '}
            · {left}% left · {resetText}
            {ageText}
          </Text>
        </Box>
      )
    })()

    // Stack above any band another plugin drew beneath this one.
    return below.type === 'engine' ? mine : (
      <Box flexDirection="column">
        {mine}
        {below}
      </Box>
    )
  })
}
