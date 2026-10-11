# Enhancement group, 2026-10-09

The routine run that opened this branch shipped #83, #99, #100, #101, #102, #103, #137 and #143,
one commit each. This file holds the design for the one issue it left as a phase.

## #104 Root motion (effort L, done)

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

### As built

These notes override the design above.

- **The delta covers only played time.** It runs from where the state stood (`PreviousTime`) to the
  step's unwrapped target (`PlayingState::UnwrappedTime`, kept by `Advance`). A seek, a new state and
  `Update(0)` move nothing, and the first update of a state counts its step. So no edge case needs a
  zeroed delta.
- **Root joint per clip.** Each `ClipBinding` picks its own root channel when it is bound: the root-most
  bound joint whose translation moves, or `SetRootMotionJoint(name)`'s. Joints above it are taken at
  their rest pose. Mode changes need no rebind. A joint change clears the bindings.
- **The hold happens in `SampleClip`,** for every clip on every layer (`HoldRoot`). Only layer 0's states
  are folded into the delta. Turning in `XZ` is the yaw of the root's model-space rotation relative to
  its rotation at the clip's start (`Yaw` = the twist about +Y). The model turns about the held root
  (`SegmentMotion`'s `Frame(start)` conjugation).
- **Application** is in `AnimationSystem::Update`, which now takes `PhysicsSync&`. It applies `local x
  delta`:
  - a controller: velocity and rotation, with `XZ` keeping the y velocity;
  - a kinematic root body: `MoveKinematic`;
  - a dynamic or static body: warns once and doesn't move;
  - otherwise the `Transform3DComponent`.

  `deltaTime` (not `x Speed`) turns the travel into a velocity, because physics steps by the scene's
  delta. A controller it stops driving (`AnimatorRuntime::DroveController`) gets its horizontal
  velocity zeroed once, and a disabled animator's delta is cleared, like its events.
- **Warnings,** once each: a root-motion joint name the skeleton lacks, and a clip that animates a
  joint above its root-motion joint, whose keys stay in the pose because the motion is measured with
  those joints at rest.
- **Tests:** the Animation Test's `RunRootMotionChecks` (15 checks) and `--anim=root`. Marionette is
  untouched, as planned.
