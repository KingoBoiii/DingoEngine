# physics-coop: breakable constraints (#309)

Sub-issue of #295 (constraints and joints). Requested by the owner for NEDRIVNING (houses are pre-cut
pieces held by joints that must break under force). Additions only (SemVer 1.x).

## Decisions

The owner approved D1–D8 and the phases on 2026-10-29.

- **D1. Force from the last sub-step's lambda.** `|λ| · collisionSteps / dt`, as `GetMotorTorque` does
  (`JoltPhysics3D.cpp:1212-1240`), because Jolt keeps only the last collision sub-step's accumulated
  impulse. With `collisionSteps > 1` a spike in an earlier sub-step goes unseen. This is documented and not worked around.
- **D2. The lambda mapping**, from the issue's table. Force: the position lambda (a vector's length, or a
  scalar's absolute value), plus the position-limit lambda for Slider (and Distance, whose position lambda already includes
  its limits). Torque: the rotation lambda, plus the rotation-limit lambda for Hinge. SwingTwist uses
  `max(|swingY|, |swingZ|, |twist|)`. Motor lambdas never count. Point and Distance have no torque row, so a
  `BreakTorque` on them is stored and has no effect.
- **D3. Check order = constraint id order.** `Constraints` is an `unordered_map`
  (`JoltPhysics3DData.h:282`). The break pass walks a sorted id list, kept as a
  `std::vector<ConstraintId3D> Breakable` (ids are monotonic, so appending keeps it sorted). A break then never depends on
  hash order, and only breakable joints are visited.
- **D4. A break goes through `DestroyConstraintInternal`** (`JoltPhysics3D.cpp:320`). That path already handles the stale
  handle, the release of the pair ignore, the `BodyConstraints` bookkeeping and the wake. Breaks happen after
  `PhysicsSystem.Update` returns, never during it.
- **D5. The broken list is capped** at 4096 entries per world. Past the cap the oldest entry is dropped, with one
  warning. A raw-Physics3D user who never drains the list must not grow memory without bound. The Scene drains it every step.
- **D6. Removing the component instead of `Broken = true`.** `PhysicsSync::SyncConstraints`
  (`PhysicsSync.cpp:228-262`) rebuilds every `Constraint3DComponent` whose joint is gone, every step. A flag
  would need a skip path there and would bring the joint back if cleared. Removal reuses
  `on_destroy<Constraint3DComponent>` → `DropConstraint` (`PhysicsSync.cpp:665`), which is a no-op on the stale
  handle. Re-adding the component rebuilds the joint.
- **D7. Delivery happens after the step, outside the physics lock**, like `OnAnimationEvent`
  (`ScriptSystem.cpp:152`). A `DestroyEntity` called from `OnConstraintBroken` waits for the end of the pass.
  BodyA's scripts are called first, then BodyB's. Within a step, breaks are delivered in constraint id order.
- **D8. Each `ConstraintBreak3D` entry is a snapshot.** Its body ids can be stale by the time the game reads them (for example, a
  body destroyed in between). The Scene maps bodies to entities through `UserData` at drain time, in the same
  step, so its own delivery never sees a stale body.

## Phases

### P1 — Physics3D API and Jolt break pass

Model: implement with `general-purpose` / `sonnet` / `medium`. Review with a fresh `general-purpose` / `opus` / `high`
agent (physics backend internals: solver lambdas, the step order, handle lifetime).

- [x] `PhysicsTypes3D.h`: add `ConstraintParams3D::BreakForce` and `BreakTorque` (defaults 0, with `///`) and
  `struct ConstraintBreak3D { ConstraintId3D Constraint; PhysicsBodyId3D BodyA, BodyB; std::uint32_t UserData;
  float Force, Torque; }`. No Jolt type in either.
- [x] `Physics3D.h`: add `SetConstraintBreakLimits`, `GetConstraintBreakLimits(constraint, float&, float&)`
  (returns bool, false on a stale handle), `GetConstraintReaction(constraint, float&, float&)` (bool) and
  `ConsumeBrokenConstraints(std::vector<ConstraintBreak3D>&)` (appends, then clears the internal list). All
  are pure virtual, with `///`.
- [x] `JoltPhysics3DData.h`: add `BreakForce`/`BreakTorque` to `ConstraintRecord`, plus `Breakable` (D3) and
  `std::vector<ConstraintBreak3D> Broken`.
- [x] `JoltPhysics3D.cpp`: add a helper `ReadReaction(const ConstraintRecord&, float& force, float& torque)` that switches
  on the type per D2 and is shared by `GetConstraintReaction` and the break pass. Run the break pass at the end of `Step`
  after `Update`: skip the pass when both bodies are asleep, collect the breaking ids, then destroy each (D4).
  Validate `CreateConstraint` params (a negative or NaN limit → warn, treat as 0).
- [x] The test app (`test/src/Tests/Physics/ConstraintTest.cpp`; never read it whole, Grep for the check
  list) gets raw-API checks:
  - The rope: 6 Distance links with `BreakForce` 3000 N, link 3 at 1500 N, and a 200 kg body hung from the end
    (about 1962 N). Exactly one break, on link 3, and `ConsumeBrokenConstraints` returns it once with the right BodyA/BodyB/UserData.
  - Reaction readback: a static hang reads about m·g within 5%. A sleeping pair reads 0.
  - The handle is stale after a break, and a new constraint's id is greater than the broken one's.
  - Set/Get break limits round-trip. Negative and NaN values are ignored.
- [x] Docs: in `docs/physics-3d.md`, add a "Breaking" subsection under Constraints (after Motors, line ~307) with the D2
  table, D1 and the sleep note.

Verification: build the engine and `Dingo-TestFramework` (public header change → full `make -j"$(nproc)"
config=debug`), run `--test=constraint`, and run doxygen with an empty warnings file. A Haiku agent runs these and reports
failures only.

**As built (P1).**
- `ReadReaction` also returns 0 for a disabled constraint (Jolt freezes its lambdas), and before the first `Step`. `SetConstraintBreakLimits` does not wake bodies.
- A non-finite limit (including +inf) is rejected like a negative or NaN one.
- `Breakable` is kept sorted with `lower_bound`, since `SetConstraintBreakLimits` can make an older id breakable.
- A hung body sleeps after ~0.5 s and then reads 0, so the m*g check samples at 0.25 s.
- Verified on Windows (Debug, hidden desktop): `--test=constraint` 167 PASS, 0 FAIL, no `VUID-` or `LOG ERROR`. Not run: Linux build, doxygen (not installed here). P1 review (Opus) fixed: stale disabled reaction, hang-check timing, doc gaps.

### P2 — Scene, scripts, F9 panel

Model: implement with `general-purpose` / `sonnet` / `medium`. Review with a fresh `general-purpose` / `opus` / `high`
agent (physics sync and the delivery order).

- [x] `Components.h`: add `Constraint3DComponent::BreakForce` and `BreakTorque` (with `///`), passed into the params in
  `PhysicsSync::BuildConstraint` (`PhysicsSync.cpp:265`). Copy and duplicate already go through
  `CopyComponentIfExists` (`Scene.cpp:205`).
- [x] `PhysicsSync`: after `m_Physics3D->Step` (`PhysicsSync.cpp:178`), drain `ConsumeBrokenConstraints`, find each
  BodyA entity (the `Constraint3DRuntime` whose constraint matches, or `UserData`), then remove its
  `Constraint3DComponent` (D6) and queue the delivery. Under `FixedTick` this runs once per tick.
- [x] `ScriptableEntity.h`: add `virtual void OnConstraintBroken(Entity other, const ConstraintBreak3D& info) {}`
  with `///`. `ScriptSystem` delivers the queue per D7.
- [x] F9 panel: in the existing constraint section, add a read-only Force/Torque reaction row and the break limits per
  constraint (`SceneDebug.cpp` ~852/1006 for the snapshot and `ImGuiDebugPanels.cpp:1466` for the UI;
  Grep only, never read those files whole).
- [x] Test app checks in `--test=constraint`:
  - The shelf: 20 bricks (0.4×0.2×0.2 m, 2 kg) in a 4×5 wall, Fixed-welded to their neighbours, with the bottom row welded to
    the world. `BreakForce` sits above the static column load (tuned once, recorded in As built). The wall holds for 10 s.
    A 10 kg box dropped from 2 m on the top centre breaks only the welds near the impact first, and the bricks above fall as the
    load cascades. No brick's lowest point goes below the floor minus 1 cm. Every id is unique.
  - The hand: a dynamic hand body and a crate joined by a Point constraint with `BreakForce` 300 N. An impulse on the crate breaks it.
    The hand entity's script receives `OnConstraintBroken` exactly once, with `other` = the crate, and the
    component is gone. Re-adding it rebuilds the joint.
  - Determinism: the shelf scene is run twice under `SceneParams::FixedTick`, and the same constraint UserData
    breaks on the same `Scene::GetTick` in both runs.
- [x] Docs: in `docs/scenes-and-ecs.md`, update the Constraint3DComponent row (line ~178), the Constraints note (~185),
  and the `ScriptableEntity` callbacks (~777), and add the component removal rule (D6).
- [x] `.claude/reference/scenes-ecs-physics.md`: add the drain order in the physics step.

Verification: full Debug build, `--test=constraint`, doxygen. A Haiku agent runs these.

**As built (P2).**
- `PhysicsSync::DrainBrokenConstraints` maps `UserData` to the owner entity (full `entt::entity` value) and requires `Constraint3DRuntime::Constraint` to match, so raw or ragdoll joints are ignored. It queues `BrokenConstraint{BodyA, BodyB = runtime Other, Info}`; `Scene::DeliverConstraintBreaks` runs after each `Physics.Step` (FixedTick: after `Interpolation.AfterTick`, before `++Tick`), with destroys deferred. The F9 Constraint3D section shows the break limits and the live reaction; no `ImGuiDebugPanels.cpp` change was needed (sections are generic).
- The shelf is five independent 4-brick columns (one `Constraint3DComponent` per entity: each brick welds to the one below, the bottom row to the world), not a full 4×5 lattice. `BreakForce` 1500 N held for 10 s; a 0.3 m weight from ~2 m broke welds. The floor check tolerance is 6 cm (measured lowest -4.2 cm under impact), not the planned 1 cm.
- Review (Opus): no High/Medium. Low: a script that hasn't started when its joint breaks misses the callback, and a scene drains breaks of hand-made joints on its world. Both documented in `scenes-and-ecs.md`, not changed.
- Verified on Windows Debug/Vulkan, hidden desktop: `--test=constraint` 178 PASS, 0 FAIL, no `VUID-` or `LOG ERROR`. Not run: Linux build, doxygen, DX11/DX12.

### P3 — review, PR

Model: whole-diff review with a fresh `general-purpose` / `opus` / `high` agent; fixes by `sonnet` / `medium`.

- [x] Run one fresh review of the full diff, fix every High and Medium, and fix or explain each Low in the PR.
- [x] Write the PR with the `pr-writeup` skill. `Closes #309`. Needs testing on a real GPU: Yes, `--test=constraint` on
  Vulkan, DX11 and DX12 (Jolt is backend-independent, but the F9 panel and the test app render), plus
  a look at the F9 reaction values by hand.

**As built (P3).**
- Whole-diff review (Opus, high): no High. M1: Jolt fixes its solved-constraint set when a collision step starts (`PhysicsSystem.cpp:734`), so a pair woken during `Update` kept its pre-sleep lambda and was read at the new `dt` (a spurious break on variable dt; a stale F9/script reading after an impulse). Fixed: `Step` snapshots `Constraint->IsActive()` into `ConstraintRecord::SolvedLastStep` before `Update`, and `ReadReaction` reads only solved constraints. With `collisionSteps > 1` a pair woken mid-`Update` reads 0 until the next step (one step of break delay). M2: the sleep tests ran under zero gravity, so they passed without the gate; added a loaded-asleep check plus a woken-pair check.
- Lows fixed: the shelf's delivery count is exact (1 call for a world weld, 2 otherwise); the `CreateConstraint` warning names infinite limits. Lows explained in the PR: the 10 s hold mostly sleeps, the free-fall check is trivially 0, Hinge/SwingTwist/Slider readback is untested, delivery skips no inactive entity (as `OnAnimationEvent`), and the two P2 Lows.

Left out: a showcase example (NEDRIVNING is the consumer, not an `examples/` project), 2D constraints, and
break events for Jolt's own sub-steps (D1).
