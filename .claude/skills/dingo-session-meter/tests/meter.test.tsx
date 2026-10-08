import { expect, mock, test } from 'claude-code/testing'
import type { On } from 'claude-code'
import type { Engine } from 'claude-code/testing'

const SURFACES = ['terminal', 'desktop'] as const

const BAND = {
  hasSurvey: false,
  isWorking: false,
  maxRows: 10,
  bodyColumns: 120,
  scroll: { offset: 0, bodyRows: 9 },
  view: {},
}

function stubBeneath(on: On) {
  on('ui.render', () => ({ type: 'engine', ref: 0 }))
  on('session.measure', (_$, e) => ({ changed: e.changed }))
  on('turn.step', async function* () {
    return {
      turnId: 't1',
      index: 0,
      answer: 'hi',
      toolUses: [],
      stopReason: 'end_turn',
      usage: {
        model: 'claude-opus-5-5',
        input_tokens: 1_000,
        output_tokens: 200,
        cache_read_input_tokens: 90_000,
        cache_creation_input_tokens: 9_000,
      },
    }
  })
}

async function respond($: Engine) {
  const stream = $.turn.step({ turnId: 't1', index: 0, model: 'claude-opus-5-5', messageCount: 2 })
  for await (const _ of stream) {
    // drain
  }
}

async function bandText($: Engine, surface: (typeof SURFACES)[number]) {
  const ui = await $.ui.mount({ plugin: 'dingo-session-meter', surface, component: 'AbovePrompt', props: BAND })
  const texts = await ui.findAll({ type: 'Text' })

  return texts.map(t => t.text).join('')
}

test('shows context fill and a live cache with its countdown', async ($, on) => {
  const clock = mock.clock(on, { now: 1_000_000 })
  stubBeneath(on)

  await $.session.measure({
    context: { tokens: 100_000, window: 200_000, percent: 50 },
    rateLimits: [],
    changed: ['context'],
  })
  await respond($)
  await clock.advance(20 * 60_000)

  for (const surface of SURFACES) {
    const text = await bandText($, surface)
    expect(text).toContain('50%')
    expect(text).toContain('100k / 200k')
    expect(text).toContain('live')
    expect(text).toContain('cold in 40m')
  }
})

test('keeps a band another plugin drew beneath it', async ($, on) => {
  mock.clock(on, { now: 1_000_000 })
  on('ui.render', () => ({ type: 'Text', props: {}, children: ['5h limit below'] }))

  for (const surface of SURFACES) {
    const text = await bandText($, surface)
    expect(text).toContain('context')
    expect(text).toContain('5h limit below')
  }
})

test('reports the cache cold once the TTL passes', { options: { cacheTtl: '5m' } }, async ($, on) => {
  const clock = mock.clock(on, { now: 1_000_000 })
  stubBeneath(on)

  await respond($)
  await clock.advance(6 * 60_000)

  for (const surface of SURFACES) {
    const text = await bandText($, surface)
    expect(text).toContain('cold')
    expect(text).toContain('re-caches ~99k next message')
  }
})

test('draws the 5h row and the context row with the same label and bar cells', async ($, on) => {
  const clock = mock.clock(on, { now: 1_000_000 })
  mock.store(on)
  stubBeneath(on)

  await $.session.measure({
    context: { tokens: 100_000, window: 200_000, percent: 50 },
    rateLimits: [
      { kind: 'five_hour', percentUsed: 3, resetsAt: new Date(clock.now() + 4 * 60 * 60_000).toISOString() },
    ],
    changed: ['context', 'rateLimits'],
  })

  for (const surface of SURFACES) {
    const ui = await $.ui.mount({ plugin: 'dingo-session-meter', surface, component: 'AbovePrompt', props: BAND })
    const text = (await ui.findAll({ type: 'Text' })).map(t => t.text).join('')
    expect(text).toContain('3% used')
    expect(text).toContain('resets in 4h 0m')

    const labels = await ui.findAll({ type: 'Box', text: /^(5h limit|context)$/ })
    const widths = labels.filter(b => b.props.width !== undefined).map(b => b.props.width)
    expect(widths).toEqual([9, 9])
  }
})

test('a resumed session keeps the cache live from its last response', async ($, on) => {
  const clock = mock.clock(on, { now: 10_000_000 })
  stubBeneath(on)

  on('classic.SessionStart', () => ({}))
  await $.classic.SessionStart({ source: 'resume', seconds_since_last_response: 600, context_tokens: 150_000 })

  for (const surface of SURFACES) {
    const text = await bandText($, surface)
    expect(text).toContain('live')
    expect(text).toContain('cold in 50m')
  }

  await clock.advance(51 * 60_000)
  const text = await bandText($, 'terminal')
  expect(text).toContain('re-caches ~150k next message')
})
