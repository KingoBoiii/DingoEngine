# Enhancement group, 2026-10-09

The routine run that opened this branch shipped #83, #99, #100, #101, #102, #103, #137 and #143,
one commit each. This file holds the design for the one issue it left as a phase.

## #104 Root motion (effort L, not started)

Clips play in place today. A clip's root travel should be able to move the entity (and its body or
character controller) instead of the hips, without counting it twice.

### API

- `AnimationLayer` stays as is. `Animator::SetRootMotion(RootMotionMode)` with `Off` (default, so
  every existing game and test is unchanged), `XZ` (horizontal travel and yaw; vertical stays in the
  pose, so a jump still lifts the hips) and `Full`.
- `Animator::GetRootMotionDelta()` returns the frame's `{ glm::vec3 Translation; glm::quat Rotation; }`
  in the skeleton's model space, already multiplied by the model's root transform scale (an FBX
  root's 0.01).
- `AnimatorComponent::RootMotion` (mode) and `ApplyRootMotion` (bool, default true). With it set,
  `AnimationSystem::Update` applies the delta to the entity: `Transform3DComponent` for a plain
  entity, `CharacterController3D::SetLinearVelocity` (delta / dt, in world space) for one with a
  controller, `MoveKinematic` for a kinematic body. A dynamic body ignores it (warn once).

### Extraction

- The root joint is the root-most joint with a translation track (the retargeting code already
  finds it: `Animator.cpp` "root-most moving joints"); `Animator::SetRootMotionJoint(name)` overrides.
- Per state in `Advance`: sample the root track at the previous and new time, wrapping (on a loop
  wrap, `end - prev` + `new - start`, plus whole laps for a step longer than the clip). Blend1D
  states lerp their two sides' deltas by the blend weight, on the shared phase.
- Fold the per-state deltas with the same weights the pose fold uses (layer 0 only; upper layers
  never move the root).
- Then pin the root joint's local translation (and yaw for `XZ`) to its value at the clip's first
  key so the pose stays in place: that is what stops the travel counting twice.
- Retargeted clips: scale the delta by the same rest-offset ratio `RetargetTranslation` uses.

### Edge cases

- Seeks (`MarkSeek`) and the first update of a state produce no delta.
- `Evaluate()` without time produces no delta.
- A rebind (`SetSkeleton`) clears it.
- A model hot-reload that shrinks a clip wraps before stepping, as events do.
- Socketed children follow automatically, since the parent's transform moves.

### Tests and docs

- Animation Test `--anim=root`: a Fox walking with `XZ`. The checks:
  - its world X advances by the clip's travel per lap within 1 %;
  - the hips' model-space XZ stays within 1 cm of the first key;
  - `Off` is bit-identical to today.
- docs/animation.md: replace the "Not in v0.8" row with a section, and update CLAUDE.md's Animator
  paragraph.
- Marionette could drop its "pay the hips travel over the fade-out" workaround, but leave that to a
  separate change, since its `--check` and `--tournament` results must not move.
