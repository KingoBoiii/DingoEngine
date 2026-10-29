# physics-coop: active ragdolls (#298)

Started by a routine run (Mode B, group `physics-coop`). The owner wasn't there, so the decisions
below are the run's; each is also in the PR's "Questions I answered myself".

## Decisions

- **D1. Point joints become SwingTwist.** The issue asks for "Point with a small cone" on minor joints,
  but a Point constraint has no limits. Minor joints get a SwingTwist with a 25° cone and ±15° twist.
- **D2. Bodies are built from the current animated pose**, not the bind pose, so the character doesn't
  snap at build. Joint limits are measured from that pose.
- **D3. `BlendGlobalTransforms` works in `GetGlobalTransforms`' space** (relative to the skeleton's
  root transform), not model space, so it composes with what the animator already exposes.
- **D4. No `AnimatorComponent` means the rest pose.** The ragdoll owns a rest-pose `Animator`, which
  the skinned draw falls back to.
- **D5. `Recipe` waits for P2.** No unused field ships in P1.
- **D6. The root drive is a velocity**, mixed toward the animated root by `clamp(Strength, 0, 1)`,
  rather than a world constraint, so a driven ragdoll can still be shoved.
- **D7. Mass needed an engine change:** `RigidBodyParams3D::Mass` (dynamic bodies only; 0 keeps the
  density mass).

## Phases

### P1 — component, auto build, motors, pose blend (done)

`RagdollComponent`, `RagdollRuntime`, `Internal::RagdollSystem`, `Animator::BlendGlobalTransforms`,
`Scene::GetRagdollBody`, `RigidBodyParams3D::Mass`, the lifecycle (destroy, remove, `SetActive`,
`OnStop`/`OnStart`, skeleton id or revision change), docs, and the Ragdoll Test (`--test=ragdoll`).

As built:
- Selection: the root is the first hip/pelvis-named joint with two or more kept children, else the
  first joint (not folded by name) with two or more such children. Below
  it, a joint gets a body if it has a child, its name doesn't fold it (finger/thumb/index/middle/ring/
  pinky/toe/eye/jaw/tongue/twist/slot/socket/ik_/_ik/pole), its bone is longer than a minimum, and at least 0.3×
  its nearest simulated ancestor's bone. The main child is the one with the most descendants. Folded
  joints map to their nearest simulated ancestor (`PartOfJoint`). More than 20 bodies is an error.
- Capsules run from the joint to its main child: radius `max(0.22L, min(0.5·lateral spread, 0.6L))`,
  half height `max(L/2 − r, 0.05L)`. Mass is shared by capsule volume.
- Hinge names: forearm/lowerarm/lower_arm/elbow/knee/calf/shin/lowerleg/lower_leg, and a name ending
  in "leg" without "up" or "thigh" (Mixamo's `LeftLeg`). When the bone is
  bent more than 10°, the axis is `cross(parent dir, child dir)` with limits `[−bend − 5°, 150° − bend]`;
  otherwise it is the joint's X axis with ±150°. SwingTwist limits by name class: torso 30/30° ±20°,
  arm 70/70° ±45°, leg 60/45° ±30°, other 25/25° ±15°.
- Motors: `k = I·ω²` with ω = 2π·6 Hz and I the subtree's inertia about the joint. Stiffness `k·S`,
  damping `2·I·ω·√S`, max torque `2·k·S`.
- Order: `PhysicsSync::Step` → `RagdollSystem::Sync` after `SyncConstraints`. `Scene::OnUpdate` →
  `PhysicsSync::PoseRagdolls` after the per-frame step.
- The Fox builds 16 bodies (the head rides on the neck; hands, Foot02, Tail03 are leaves).
- Collision: a ragdoll's parts ignore the entity's own body and controller and those of its
  ancestors (`PhysicsSync::SyncAncestorFilters`); `GetSensorOverlaps` reports the entity once.
- Fixed tick: the drive uses the previous frame's animation and the pose reads uninterpolated bodies
  (documented in docs/animation.md).

### P2 — recipes and hot-reload (done)

- [x] A `.ragdoll` sidecar (per-joint shape, radius, limits, mass share, excluded joints) loaded beside
  the model, and `RagdollComponent::Recipe` to name one. Radii from vertex weights when no recipe gives
  them. A model reload already rebuilds (skeleton revision); add a check that a recipe edit does too.

As built:
- One file per model, `<stem>.ragdoll`, with `[name]` sections; the lines before the first section are
  the default recipe. `RagdollComponent::Recipe` names a section (empty = the default), not a file, so
  the AssetManager's watch covers every recipe a model has. Format and keys: docs/animation.md,
  "Ragdoll recipes". Parsed by `Internal::ParseRagdollRecipes` (`Graphics/RagdollRecipeFile.h`, doctest
  `RagdollRecipeFile`), sharing the `.events` tokenizer (`Internal::TokenizeSidecarLine`).
- `Model::GetRagdollRecipes/FindRagdollRecipe/ReloadRagdollRecipes/GetRagdollRecipeRevision`. The
  revision moves on every load, reload and `Adopt`; with the recipe name it joins the rebuild key
  (`RagdollRuntime::Recipe/RecipeRevision`) and the rejection key.
- The AssetManager's single companion became a list (`CompanionFile { Extension, Reload }`, the model
  row `.events` → `ReloadEvents`, `.ragdoll` → `ReloadRagdollRecipes`); a change to the model file
  itself still runs one full `ReloadInPlace`.
- `radius=` is in model units (the space of `Mesh::GetVertices()`), scaled per bone by its world/rest
  length. Hinge limits are degrees from the bone lying straight, re-centred on the build pose's bend.
  Mass shares are exact when they leave room; the rest is split by size, and shares of 1 or more are
  scaled with the others (warned).
- Measured radius: the 85th-percentile distance (first built as the median, which left the skin clipping into the floor) of the rest vertices each part moves most, from its bone,
  clamped to 0.08–0.6 × length, at least 8 samples, else the P1 rule. **Mass still splits by the P1
  automatic capsule volume**, not the measured collider: with measured volumes the Fox's fluffy tail
  tip weighed 12 kg on a 0.4 kg base and the torso outweighed the hips 5:1, and the driven hip drifted
  0.47 m (the check allows 0.05). Clamping neighbour mass ratios or adding an integral term to the root
  drive didn't hold it either. Now 0.034 m.
- Left out: an `include` key (forcing a body on a folded joint) and a hinge axis override.

### P3 — showcase and tooling (done)

- [x] The issue's full scene test: a push that topples a driven character, a fall down stairs, and 8
  ragdolls at once with a frame-time budget. The F9 Scene tab shows a ragdoll's bodies, motors and
  Blend/Strength live. Interpolate the posed bodies under `FixedTick`.

As built:
- Test (`RagdollTest`): the push checks bend and recovery rather than a topple (a driven fox at
  `Strength` 1 must come back; the `Strength` 0 fox collapses): a 60 N·s shove on the chest bends it
  over 8 mm and it ends within 5 mm of its pose after 2 s (measured 14.2 / 0.40 mm). A driven fox
  carried at 1.5 m/s keeps its hip within 4 cm (25 mm, one frame of the previous-frame target). A limp
  fox shoved 150 N·s off a 1.44 m landing must leave it and drop 0.5 m. Eight foxes (128 bodies, 120
  joints) log their `Scene::OnUpdate` time (Debug ~4.2 ms mean) and assert an 8 ms mean outside Debug
  and ASan. A mesh collider added after `OnStart` needs `Scene::CreateRigidBody`.
- **Motors are 3 Hz, not 6** (`k_DriveFrequency`): at 6 and 4 Hz the Fox's light tail and limb
  bodies gained energy in Jolt's solver and the driven pose swung by centimetres (it predated P3,
  hidden by the old sampling); 3 and 2 Hz settle. *Follow-up (#307):* the cause was a 1 kg tail
  root whose subtree holds 95× its own inertia, so the subtree-scaled spring rang that body alone
  at ~58 Hz against a 60 Hz step. Each motor's stiffness is now capped at its own body's inertia
  about the joint × (2π · 0.25 / dt)², and the frequency is `RagdollComponent::DriveFrequency`
  (default 3 Hz, 0 limp, F9-editable). The cap binds at 3 Hz too wherever a subtree holds over 25× its
  body's inertia (the tail root keeps ~26% of its old stiffness). The push check returns within 0.06 mm
  at 3, 6 and 10 Hz.
- **The root drive moves the whole ragdoll rigidly**: the correction (spring `min(ω, 1/dt)` plus the
  target's own velocity fed forward, skipped past a 0.5 m / 1 rad jump) is measured against the
  parts' mass-weighted mean velocity and added to every part. On the root alone the chain absorbed it
  and the body swung on a pinned hip.
- Interpolation: `RagdollSystem::AfterStep` (end of `PhysicsSync::Step`) records each part's pose and
  moves a following entity; `PoseRagdolls(alpha)` blends them inside an `InterpolatedView` (only when
  a ragdoll exists), and `PhysicsInterpolation` tracks a `FollowsRoot` entity. The animator holds the
  drawn bones, so joint reads and sockets on ragdoll bones lag up to a tick under `FixedTick`.
- F9: `SceneDebug::RagdollView`/`WriteRagdoll` and an inspector section; `Blend`, `Strength` and
  `DriveToAnimation` are editable, each body shows joint type, mass, speed and motor torque / limit.
- Recovery (found on the GPU run): a limp ragdoll carries its entity down (y -0.415 for the Fox), so
  `Strength` back to 1 aimed the pose into the floor and the hip stalled at 0.16 m. The runtime now keeps
  the entity's height above the ground under the hips when it goes limp (`HasGroundHeight`, `GroundHeight`)
  and `Drive` puts the entity back that high above the ground below the hips when it is driven again
  (`Carried`). `Ragdoll Test` +2 checks (38): hip 0.799 m standing, 0.082 m limp, 0.797 m after.
