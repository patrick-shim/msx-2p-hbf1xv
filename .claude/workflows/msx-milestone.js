export const meta = {
  name: 'msx-milestone',
  description: 'Deterministic Sony HB-F1XV milestone loop: planner -> developer -> QA, retry until QA sign-off',
  whenToUse: 'Drive one or more HB-F1XV milestones to QA sign-off with evidence gates, hands-off.',
  phases: [
    { title: 'Plan', detail: 'msx-planner produces the milestone package' },
    { title: 'Implement', detail: 'msx-developer implements the slice + runs evidence gates' },
    { title: 'QA', detail: 'msx-qa assesses regression and recommends sign-off' },
  ],
}

// ---------------------------------------------------------------------------
// Args (all optional):
//   { milestones: [{ id, title, objective }], maxRoundsPerMilestone: 3 }
// or a single milestone:
//   { id: 'M10', title: '...', objective: '...' }
// Milestones run sequentially because each depends on the prior one's closure.
// Within a milestone: plan once, then loop implement -> QA until Pass (or the cap).
// ---------------------------------------------------------------------------

const input = args || {}
const milestones = input.milestones
  ? input.milestones
  : [{ id: input.id || 'M-next', title: input.title || 'Next milestone', objective: input.objective || 'Advance the emulator by the next approved deterministic slice.' }]
const MAX_ROUNDS = input.maxRoundsPerMilestone || 3

const GATES = [
  'tools/validate-assets.ps1',
  'tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt',
  'cmake --build build --config Debug',
  'ctest --test-dir build -C Debug --output-on-failure',
]

const QA_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['decision', 'gaps', 'residualRisks', 'signoffPath'],
  properties: {
    decision: { type: 'string', enum: ['Pass', 'Conditional Pass', 'Fail'] },
    gaps: { type: 'array', items: { type: 'string' } },
    residualRisks: { type: 'array', items: { type: 'string' } },
    signoffPath: { type: 'string', description: 'docs/m<N>-qa-signoff.md written by QA' },
  },
}

const results = []

for (const m of milestones) {
  log(`=== ${m.id}: ${m.title} ===`)

  // --- Plan (once) ---------------------------------------------------------
  phase('Plan')
  const plan = await agent(
    `You are planning milestone ${m.id} ("${m.title}") for the Sony HB-F1XV emulator.\n` +
    `Objective: ${m.objective}\n\n` +
    `Follow CLAUDE.md and agent-protocol/guardrails.md. Produce the durable planner package at ` +
    `docs/${m.id.toLowerCase()}-planner-package.md with scope, dependency map, deterministic ` +
    `unit/integration/system test obligations, acceptance criteria mapped to the build/test flow, ` +
    `evidence-gate obligations (${GATES.join('; ')}), and risks. Return a concise summary and the package path.`,
    { agentType: 'msx-planner', phase: 'Plan', label: `plan:${m.id}` }
  )

  // --- Implement -> QA loop until sign-off ---------------------------------
  let round = 0
  let qa = null
  let priorGaps = []

  while (round < MAX_ROUNDS) {
    round += 1

    phase('Implement')
    const impl = await agent(
      `You are implementing milestone ${m.id} ("${m.title}"), round ${round}.\n` +
      `Planner package: docs/${m.id.toLowerCase()}-planner-package.md\n` +
      (priorGaps.length ? `Address these QA gaps from the previous round:\n- ${priorGaps.join('\n- ')}\n` : '') +
      `\nImplement the minimum slice, add/update deterministic unit + integration tests, then run ` +
      `every evidence gate and capture real output:\n- ${GATES.join('\n- ')}\n` +
      `Write docs/${m.id.toLowerCase()}-implementation-report.md and return a summary plus actual gate results. ` +
      `Never fabricate output; if a gate cannot run, say so explicitly.`,
      { agentType: 'msx-developer', phase: 'Implement', label: `impl:${m.id}#${round}` }
    )

    phase('QA')
    qa = await agent(
      `You are QA for milestone ${m.id} ("${m.title}"), round ${round}.\n` +
      `Planner package: docs/${m.id.toLowerCase()}-planner-package.md\n` +
      `Implementation report: docs/${m.id.toLowerCase()}-implementation-report.md\n` +
      `Developer summary:\n${impl || '(none returned)'}\n\n` +
      `Verify the evidence (re-read captured output; re-run a gate if in doubt). Build the regression ` +
      `matrix, rank residual risks, and write docs/${m.id.toLowerCase()}-qa-signoff.md. ` +
      `Return the decision. Mark "Pass" ONLY when no blocker-level gap remains.`,
      { agentType: 'msx-qa', phase: 'QA', label: `qa:${m.id}#${round}`, schema: QA_SCHEMA }
    )

    if (!qa) { log(`${m.id} round ${round}: QA returned nothing; stopping this milestone.`); break }
    log(`${m.id} round ${round}: QA = ${qa.decision} (${qa.gaps.length} gaps)`)
    if (qa.decision === 'Pass') break
    priorGaps = qa.gaps
  }

  results.push({ milestone: m.id, title: m.title, planSummary: plan, qa, rounds: round })

  // Sequential dependency: do not open the next milestone unless this one signed off.
  if (!qa || qa.decision !== 'Pass') {
    log(`Stopping the chain at ${m.id}: no QA Pass after ${round} round(s). Human decision needed.`)
    break
  }
}

return {
  milestonesAttempted: results.length,
  signedOff: results.filter(r => r.qa && r.qa.decision === 'Pass').map(r => r.milestone),
  results,
}
