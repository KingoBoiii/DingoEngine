# Particles

*(v0.9)*

`Renderer3D` simulates particles on the GPU: compute kernels spawn and move them in a pool of 65,536
by default, and one instanced draw per batch billboards them. Particles are unlit and colours above 1
bloom through the post chain, so sparks, embers and flames glow.

## An effect and an emitter

An **effect** is the look and behaviour, built from `ParticleEffectParams`; an **emitter** is one
running instance of it.

```cpp
ParticleEffect* sparks = ParticleEffect::Create(ParticleEffectParams()
	.SetShape(ParticleShape::Cone, { 20.0f, 0.05f, 0.0f }) // 20 degrees, from a disc of radius 5 cm
	.SetRate(800.0f)
	.SetLifetime(1.4f, 2.0f)
	.SetSpeed(4.0f, 6.0f)
	.SetGravity({ 0.0f, -9.8f, 0.0f })
	.SetStartSize(0.05f, 0.09f)
	.SetColors({ { 0.0f, { 4.0f, 2.0f, 0.6f, 1.0f } }, { 1.0f, { 0.5f, 0.1f, 0.0f, 0.0f } } }));
```

**In the ECS**, put a `ParticleEmitterComponent` on an entity with a `Transform3DComponent`:

```cpp
Entity torch = scene->CreateEntity("Torch");
torch.AddComponent<Transform3DComponent>().Position = { 0.0f, 1.0f, 0.0f };
torch.AddComponent<ParticleEmitterComponent>(sparks);

scene->EmitParticles(torch, 50);                  // a burst from the effect's shape
scene->EmitParticlesAt(torch, impactPoint, 30);   // a burst around a world-space point
```

The `SceneRenderer`'s 3D pass draws it from the entity's world transform, so an emitter parented to a
joint socket follows the joint. `Scene::OnUpdate` steps it by the scene's capped delta: a paused
scene freezes its particles. `Playing` (stop: no more spawns at the rate, the particles live out
their lives; start again: the effect's `BurstOnPlay`), `RateScale` and `WorldSpace` (off: the
particles move with the entity) are on the component. Every `Renderer3D` that draws the scene (a
custom pass through `Scene::RenderEntities3D`, say a minimap, beside the `SceneRenderer`) runs an
emitter of its own for the entity, since a renderer draws only its own pool: each simulates on its
own and takes every burst, and one no pass has drawn for 300 frames gives its ring back.

**Without the ECS**:

```cpp
std::shared_ptr<ParticleEmitter> emitter = renderer3D.CreateParticleEmitter(sparks);
// every frame, between BeginScene and EndScene:
renderer3D.SubmitParticles(*emitter, transform, deltaTime);
emitter->EmitAt(point, 20); // spawns at the next submission
```

Submit an emitter once a frame; a second scene that shows it (a minimap) submits it with a
`deltaTime` of 0, or it steps twice. Dropping the last `shared_ptr` returns its ring to the pool.

## From animation events

The `.events` files beside a model already say *when* a foot lands or a blade is live
([Events](animation.md#events)); a `ParticleEventComponent` on the animated entity says *what*:

```cpp
Entity dust = scene->CreateEntity("Left foot dust");
dust.AddComponent<Transform3DComponent>();
dust.AddComponent<ParticleEmitterComponent>(footDust);
dust.SetParent(fighter, "foot_l");                  // a joint socket: the dust follows the foot

Entity trail = scene->CreateEntity("Sword trail");
trail.AddComponent<Transform3DComponent>();
trail.AddComponent<ParticleEmitterComponent>(swordTrail).Playing = false;
trail.SetParent(fighter, "hand_r");

fighter.AddComponent<ParticleEventComponent>()
	.Bind("step_l", dust.GetUUID(), 6)               // an instant: a burst of 6
	.BindRange("hitbox", trail.GetUUID());           // a range: the emitter plays while it is open
```

- **Exactly what the scripts hear.** The bindings take the same events `OnAnimationEvent` gets, each
  frame after the animate pass: the dominant clip's on each layer, so a footfall fires once even
  across a blend.
- **Ranges stop on `RangeEnd`**, including the ones a rebind, a seek or a removed `AnimatorComponent`
  sends; removing the `ParticleEventComponent` (or its entity) stops every range emitter it binds.
  Start a range's emitter with `Playing` off. A range that opens and closes within one frame (a short
  range, a low frame rate, a seek) never reaches the emitter as playing, so it gets the effect's
  `BurstOnPlay` as a burst instead.
- **Live edits move them.** Editing a `.events` file while the game runs re-times the bursts through
  the model's hot-reload ([Model hot-reload](animation.md)).
- Emitters are found by UUID. `Scene::DuplicateEntity` points a copied binding at the copy of its
  emitter when the emitter is inside the duplicated subtree (a fighter's socketed dust); a binding to
  an emitter outside it still names that one.

*Marionette* (`examples/Marionette/src/Fighter.cpp`, `BuildVfx`) is the worked example. Each fighter
has dust emitters on its `foot.l` and `foot.r` sockets and at its feet, and a trail emitter at its
blade's tip, parented to the weapon. Its `ParticleEventComponent` binds `step_l`/`step_r` to a burst
at that foot, `dash` to a larger one, and the `hitbox` range to the trail. No game code runs per
event. Its impacts go the other way: `Combat` bursts sparks with `Scene::EmitParticlesAt` at the
point where a blade met its target (`ArenaVfx`).

## ParticleEffectParams

| Field | Default | Meaning |
|---|---|---|
| `Shape`, `ShapeSize` | `Point` | Where particles start and which way they leave: `Point` (every direction), `Sphere` (inside radius `x`, outward), `Cone` (along +y within `x` degrees, from a disc of radius `y`), `Box` (inside half extents `xyz`, along +y). In the emitter's space. |
| `Rate`, `BurstOnPlay` | 10, 0 | Particles a second while playing; how many at once when it starts. |
| `Lifetime`, `Speed` | 1..1 s, 1..1 | Picked per particle between the two. |
| `Gravity`, `Drag` | 0, 0 | Acceleration; speed lost as `exp(-Drag * t)`. |
| `InheritVelocity` | 0 | The part of a world-space emitter's own velocity a particle starts with. |
| `NoiseStrength`, `NoiseScale` | 0, 1 | A swirling push from a divergence-free (curl) field, so particles swirl without bunching. |
| `StartSize`, `EndSize` | 0.1..0.1, 1 | Size across in world units, and its factor at death. |
| `StartRotation` | 0..0 | Degrees, picked per particle. 0..0 keeps a sprite upright; 0..360 scatters it, as smoke and sparks want. |
| `Spin` | 0..0 | Degrees a second. |
| `ColorKeys` | white, fading out | Up to four HDR keys over life (0 birth, 1 death). |
| `Blend` | `Additive` | `Additive` adds light and needs no sorting; `Alpha` draws over what is behind, unsorted. |
| `Texture`, flipbook | none | A sprite times the colour (null draws a soft round dot); with columns and rows, the frames play over each particle's life. |
| `SoftDistance` | 0.25 | Through the post chain, a particle fades out within this distance of what is behind it, so it doesn't cut a line into the floor. 0 = a hard edge. |
| `Capacity` | auto | The most particles one emitter keeps: `Rate` x the longest `Lifetime` x 1.1 + `BurstOnPlay` + 64 when 0. Spawns past it in one step are dropped; later ones overwrite the oldest. |

`ParticleEffect::SetParams` takes effect for every emitter on its next frame, except `Capacity`,
which an emitter takes when it is made. An effect must outlive its emitters.

## How it works

- **The pool.** One storage buffer per `Renderer3D`, `Renderer3DCapabilities::MaxParticles` (65,536,
  48 bytes each: 3 MB) made with the first emitter. Each emitter owns a ring of it; one the pool has no
  room for draws nothing and warns once.
- **Each scene**, `EndScene` uploads a record per emitter (its transform, the step's delta, the
  effect) and the spawns, then dispatches **simulate** (age, gravity, drag, noise; a particle past its
  lifetime dies) over every slot of the scene's emitters, then **emit** over the new particles. The
  CPU owns each ring's head, so there are no atomics.
- **The draw** follows the opaque meshes: per run of emitters sharing a blend and sprite, one draw
  instanced over their slots, depth-tested without writing depth; a dead slot draws nothing.
- **With the post chain**, ambient occlusion is applied before the particles (so they aren't
  darkened) and soft particles read a copy of the scene depth (`PostProcessStack::CopySceneDepth`).
  Without it they draw with hard edges.
- **Cost**: every slot of every submitted emitter is simulated and drawn each scene, alive or not, so
  keep capacities near what an effect needs. `Particles` in the F8 Profiler tab times it; F4's
  Renderer tab shows the pool, emitters, slots, spawns and dropped spawns.

## Checking it

The test app's **Particle Test** (`--test=particles`, `--particles=fountain|burst|soft|budget|events`)
shows a fountain of sparks through bloom, bursts at random points, soft smoke against the floor, an
emitter keeping about 30,000 alive, and the Fox puffing dust at every footfall. Over its first five frames it checks by reading the pool back:
an emitter takes the whole pool and keeps every particle alive while another finds no room; a burst
of 100 gives 100 live particles and none outlives its lifetime; a ring of 64 asked for 100 keeps 64
and drops 36; a `ParticleEmitterComponent`'s burst through the `SceneRenderer` lands in its emitter;
and a soft particle fades where it meets the floor while its top draws as a hard one does. Over the
next 300 fixed frames, through a `ParticleEventComponent` on the Fox, every footfall a script hears
across a Walk-to-Run blend bursts exactly once, and Survey's `look` range plays its emitter only while
it is open.
