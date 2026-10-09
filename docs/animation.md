# Animation & Skinned Models

*(v0.8)*

DingoEngine plays skeletal animation. A `Model` loaded from a glTF or FBX file brings its
`Skeleton`, its skin weights and its `AnimationClip`s. `Renderer3D` skins the meshes on the GPU from a
joint palette, and an `Animator` plays and blends the clips into that palette. Timeline **events**
and joint **sockets** tie the animation to gameplay: a footstep sound the moment a foot lands, a
hitbox that is open between two moments of a swing, a sword that follows the hand. The melee duel
`examples/Marionette` uses every piece; the sections below point to it where it is the worked example.

> **Two ways to use it.** In the Scene/ECS, give an entity a `SkinnedMeshRendererComponent` and an
> `AnimatorComponent`, and the scene draws and advances it ([Drawing in a scene](#drawing-in-a-scene),
> [AnimatorComponent](#animatorcomponent)). Outside the ECS, drive an `Animator` yourself and draw with
> `Renderer3D::SubmitSkinnedMesh` ([Drawing without a scene](#drawing-without-a-scene),
> [A standalone Animator](#a-standalone-animator)). Both follow the same rules below.

| Piece | Header | What it does |
|---|---|---|
| `Model` | `Graphics/Model.h` | A loaded file: submeshes and, when it has bones, a `Skeleton` and its clips. |
| `Skeleton` | `Graphics/Skeleton.h` | The joints: name, parent, rest pose, inverse bind matrix. |
| `AnimationClip` | `Graphics/AnimationClip.h` | Keys for joints, by joint name, over seconds; plus timeline events. |
| `Animator` | `Graphics/Animator.h` | Plays and blends clips on a skeleton, holds the pose, collects events. |
| `Renderer3D::SubmitSkinnedMesh` | `Graphics/Renderer3D.h` | Skins and draws one mesh from a joint palette. |
| `SkinnedMeshRendererComponent` | `Scene/Components.h` | Draws a model in a scene, posed by the entity's animator. |
| `AnimatorComponent` | `Scene/Components.h` | Settings for the entity's animator; `Scene::GetAnimator` returns it. |
| Sockets | `Scene/Entity.h` | `child.SetParent(character, "joint")` hangs an entity on a joint. |

Everything is available through `<DingoEngine.h>`.

## A first animated character

```cpp
Model* fox = Model::LoadFromFile("models/Fox/Fox.gltf");     // nullptr if it fails to load

Material* fur = Application::Get().GetRenderer3D().CreateLitMaterial(MaterialParams().SetDebugName("Fox"));
fur->SetTexture(0, fox->GetSubMeshes()[0].DiffuseTexture);   // the Fox has one submesh

Entity entity = scene.CreateEntity("Fox");
entity.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, 0.0f, 0.0f }, glm::vec3(0.02f))); // ~150 units long
entity.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(fox)).Material = fur;
entity.AddComponent<AnimatorComponent>(AnimatorComponent("Survey"));   // plays Survey, looping
```

That is a posed, animated, textured fox: the scene draws it and `Scene::OnUpdate` advances its
animation. (You own `fox` and `fur`; delete the material before the renderer shuts down, in
`Layer::OnDetach`. A model loaded through the AssetManager belongs to the manager instead.) A script
then drives it from game state:

```cpp
class FoxScript : public ScriptableEntity
{
public:
    float Speed = 0.0f;   // set by whatever moves the fox

protected:
    void OnStart() override
    {
        const Model* model = GetComponent<SkinnedMeshRendererComponent>().Model;
        m_Locomotion = AnimationState::Blend1D("Speed", {
            { 0.0f, model->FindAnimation("Survey") },
            { 1.5f, model->FindAnimation("Walk") },
            { 4.0f, model->FindAnimation("Run") } });
    }

    void OnUpdate(float dt) override
    {
        if (Animator* animator = GetScene().GetAnimator(GetEntity()))
        {
            animator->SetFloat("Speed", Speed);      // the blend follows the parameter
            animator->Play(m_Locomotion, 0.2f);      // a no-op while it already plays
        }
    }

private:
    AnimationState m_Locomotion;
};
```

The Fox is a CC-BY 4.0 glTF in `test/assets/models/Fox/` (attribution in the file). It has 24 joints
and three clips, `Survey`, `Walk` and `Run`. Examples below that name those clips, or joints such as
`b_Head_05`, `b_Spine01_02` and `b_RightHand_08`, mean the Fox's; other clip names (`Jump`, `Slash`)
stand in for your own.

## Loading skinned models

Load a model like any other: `Model::LoadFromFile(path)`, or through the
[AssetManager](asset-pipeline.md) (`assets.Load("models/Fox/Fox.gltf")`, then `GetModel(handle)`),
which owns what it loads, so never `delete` a managed model. A relative path is looked up under the
asset root first, then the working directory. `LoadFromFile` hands the model to the caller, and
returns `nullptr` with an error logged when the file does not load. The AssetManager recognises
`.obj`, `.gltf`, `.glb` and `.fbx`; any other format assimp reads loads through `LoadFromFile`
directly.

**What makes a model skinned.** At least one of its meshes has bones, or the file has animation and no
meshes at all (a clip library, below). Then `Model::IsSkinned()` is true and `GetSkeleton()` is not
null, unless the file is odd in one of the ways [below](#when-the-file-is-odd). Anything else loads exactly as it did before v0.8: every mesh is pre-transformed into model
space, there is no skeleton, and any clips in the file are dropped with a warning.

| Call | Gives |
|---|---|
| `IsSkinned()`, `GetSkeleton()` | Whether the file gave a skeleton, and the `Skeleton` (null for a static model). |
| `GetSubMeshes()` | One `SubMesh` per skinned mesh, and per placement of a static one. `MeshData->HasSkin()` says which are skinned. |
| `GetAnimationCount()`, `GetAnimation(i)` | The clips, in file order. The `Model` owns them. |
| `FindAnimation(name)` | The first clip with that name, or null. |
| `GetFilePath()`, `GetGeneration()`, `Reload()`, `ReloadEvents()` | See [Hot-reload](#hot-reload). |

### Skeleton

A `Skeleton` is the subtree of the file's node graph under the lowest common ancestor of every bone
and every animated node. Its joints keep the file's node names, and those names are how masks,
sockets, retargeting and `FindJoint` find them.

- **Order.** Joints are ordered parents first, and the joints a skin vertex can reach come before all
  the others (helper and end joints, animated cameras and the like follow, and stay addressable for
  sockets). `GetSkinJointCount()` is therefore the size of a skinning palette, while `GetJointCount()`
  can be larger.
- **Joints.** `GetJoint(i)` returns a `Joint`: `Name`, `Parent` (an index, -1 for the root),
  `InverseBind` (takes a skin vertex into the joint's space at bind time) and `RestPose` (a
  `JointPose`: the node's local translation, rotation and scale in the file). A glTF's bind pose, which
  `InverseBind` undoes, may differ from the node's `RestPose`.
- **Lookup.** `FindJoint(name)` returns the index, or `Skeleton::k_InvalidJoint`. With duplicate names
  the first wins.
- **Root transform.** `GetRootTransform()` is the file's transform above the root joint; model space
  is `RootTransform * global`. An FBX exported in centimetres carries its 0.01 unit scale here.
- **Rest pose.** `GetRestPalette()` is the palette of the rest pose, and `GetRestGlobalTransforms()`
  the joints' transforms in it. An unanimated skinned mesh draws with the rest palette.
- **Identity.** `GetId()` is never reused (a freed skeleton's address can be), and `GetRevision()`
  moves on every [model reload](#hot-reload) that keeps the joints, which may bring new rest poses and
  inverse binds. A `Skeleton` cannot be copied.

Model units are the file's. The Fox is about 150 units long; scale the entity's
`Transform3DComponent` to fit your world.

### Skinned meshes

A skinned `Mesh` has `HasSkin()`, `GetSkinVertices()` and `GetSkinJointCount()`. Each
`SkinnedMeshVertex` (56 bytes) holds a position, normal and UV in skin space, the four heaviest joints
that move it (indices into the owning skeleton) and their weights, which sum to 1.

- **`Mesh::GetVertices()` is the rest pose**, in model space. Physics (`MeshCollider3DComponent` and
  the like) and `Renderer3D::SubmitMesh` therefore see the character standing in its rest pose, and a
  skinned mesh drawn through `SubmitMesh` shows that pose. Colliders do not follow the animation: put a
  kinematic body on a [socket](#sockets) for that.
- **`GetSkinJointCount()`** is one past the highest joint any vertex names. That is the number of
  palette matrices a draw needs, and what the [128-joint cap](#limits) counts.
- A vertex with no weights follows its mesh's first bone, with a warning. A rigid mesh the file hangs
  on a bone (a sword exported as a child of the hand) is bound to that bone with weight 1, so it
  animates with no setup. A skinned mesh that several nodes use becomes one submesh, because skinning
  ignores the node.
- A `Mesh` owns a GPU copy of its skin, made on its first skinned draw and freed in its destructor, so
  a `Mesh` cannot be copied.

### Clips

An `AnimationClip` has a name (from the file), a `GetDuration()` in seconds, and one channel per
joint it animates, each with translation, rotation and scale tracks (`Step` or `Linear`; a track is
`Step` only when all its keys are). Channels name their joint, so a clip finds its joints by name on
any skeleton that has them. A channel for a node outside the skeleton, or for a joint that is already
animated, is dropped with one warning per clip. `GetSourceSkeleton()` is the skeleton of the file the
clip came from, which [retargeting](#retargeting) reads.

Clip names come from the file. An exporter that gives every clip one name leaves `FindAnimation`
unable to tell them apart (Mixamo names each clip `mixamo.com`); use `GetAnimation(index)` there.

### Clip libraries

A file with a skeleton and clips but no meshes is a **clip library**: `IsSkinned()` is true,
`GetSubMeshes()` is empty, and `GetAnimation(i)` hands out clips any character with the same joint
names can play. Several characters can share one set of animations that way
([Retargeting](#retargeting)); keep the library loaded for as long as an animator plays its clips.
Animation-only Collada and BVH files read as clip libraries too (load them with `LoadFromFile`; the
AssetManager does not recognise those extensions). A library that also carries a preview mesh (KayKit's
hold their mannequin) loads that mesh as well; there is no clips-only option yet
([#100](https://github.com/KingoBoiii/DingoEngine/issues/100)).

See `examples/Marionette` (`GameAssets`, `Moveset`): five KayKit libraries, one per category of clip,
serve four characters.

### When the file is odd

The loader warns and carries on where it can. A bone that matches no node has its weights ignored. A
file whose bones and animated nodes cannot be found in the node graph, or whose skeleton would have
more than 65,535 joints, loads without a skeleton (`IsSkinned()` is false). A transform that cannot
be inverted (zero scale) reads as identity. FBX files with `$AssimpFbx$` pivot nodes are read a second time with the pivots
folded into the joints, so joint names stay the file's.

## Drawing skinned models

### Drawing in a scene

`SkinnedMeshRendererComponent` draws every submesh of a `Model` at the entity's world transform. The
entity needs a `Transform3DComponent` as well.

| Field | Default | Meaning |
|---|---|---|
| `Model* Model` | null | Not owned. Null draws nothing. |
| `glm::vec4 Color` | white | Tints every submesh (the lit shader multiplies it with the albedo). Alpha comes from this colour alone. |
| `Material* Material` | null | Applies to **every** submesh. Null is the built-in lit material. |
| `bool Visible` | true | `false` skips the entity. It is not inherited by children. |

```cpp
entity.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(fox, { 1.0f, 0.9f, 0.9f, 1.0f }));   // (model, color)
```

- **Skinned submeshes** draw through `Renderer3D::SubmitSkinnedMesh`, one draw each, posed by the
  entity's [animator](#animatorcomponent) or, without one, by `Skeleton::GetRestPalette()`. Submeshes
  without a skin draw like a `MeshRendererComponent`.
- **The material** replaces the submeshes' own: a `SubMesh`'s `Mat` and `DiffuseTexture` are not
  used. Build a lit material and give it the texture, as above.
- **Materials.** A lit material (null, or from `Renderer3D::CreateLitMaterial`) draws through a
  skinned copy of itself that the renderer keeps, so emissive, specular and the albedo texture work
  as on static meshes ([Lighting](lighting.md)). Skinned lit draws show both faces.
- **Rendering.** The `SceneRenderer` draws it. A custom pass calls `scene.SubmitLights(renderer)` and
  `scene.RenderEntities3D(renderer)` between `BeginScene` and `EndScene`, as for any 3D entity.

### Instances and the per-frame budget

Every skinned draw reads its joints from a `SkinData` uniform buffer, written once per **instance**.
An instance is a run of consecutive `SubmitSkinnedMesh` calls with the same palette (the same array of
matrices), transform and colour, which is what a model's submeshes are. Each instance uploads its
joints once and counts once against `Renderer3DCapabilities::MaxSkinnedInstances`.

```cpp
ApplicationParams params;
params.Renderer3D.Capabilities.MaxSkinnedInstances = 128;   // default 64, at most 256
params.Renderer3D.Capabilities.AssertOnOverflow = true;     // assert instead of warn-and-drop
```

- The budget is **per frame**, across every scene the renderer runs, because each instance holds
  a version of the volatile buffer on Vulkan. `Renderer3D::GetSkinnedInstanceBudget()` returns the
  value in force, clamped to 1 and `Renderer3D::k_MaxSkinnedInstancesLimit` (256).
- An instance past the budget is dropped **whole**, so a character never loses some of its parts.
  The renderer warns once and counts the dropped draws in `Statistics::DroppedSkinnedDraws`.
  `AssertOnOverflow` makes it an assert, which compiles out of release builds.
- The budget counts instances, not draws: a character of five submeshes is one instance and five
  draws. With the default of 64, 64 foxes draw and a 65th is dropped.

### Draw order and translucency

Opaque skinned meshes draw **after every opaque static batch** in `EndScene`, in submission order,
writing depth. A skinned mesh with a translucent material (`MaterialParams::Translucent`) draws in
the translucent pass instead, after all of them: sorted far to near together with the translucent
static meshes, by the centre of its rest bounds, without writing depth. So a character behind a glass
pane shows through it, and a ghost blends over the room. The sort is per mesh, so a character's
translucent parts can blend in the wrong order with each other; see
[Translucent materials](lighting.md#translucent-materials).

### Drawing without a scene

`Renderer3D::SubmitSkinnedMesh(mesh, transform, joints, color, material = nullptr)` draws one mesh
between `BeginScene` and `EndScene`. `joints` is a palette with at least `mesh->GetSkinJointCount()`
matrices, from any of:

- `Animator::GetSkinningPalette()`,
- `Skeleton::GetRestPalette()`,
- your own joint poses, through `Skeleton::ComputeGlobalTransforms(localPoses, globals)` (one matrix
  per joint) and `Skeleton::ComputeSkinningPalette(globals, palette)` (one per skin joint).

```cpp
Animator animator(model->GetSkeleton());
animator.Play(model->FindAnimation("Walk"), 0.25f);

// Each frame:
animator.Update(dt);

renderer.BeginScene(camera);
for (const SubMesh& submesh : model->GetSubMeshes())
    renderer.SubmitSkinnedMesh(submesh.MeshData, transform, animator.GetSkinningPalette(), glm::vec4(1.0f), material);
renderer.EndScene();
```

The renderer copies the palette during the call. Pass the same palette for a model's submeshes, with
the same transform and colour, so they share one instance.

A mesh with no skin goes through `SubmitMesh` instead (silently, in its rest pose). So does a mesh that
uses more than `Renderer3D::k_MaxSkinJoints` (128) joints, or that is given fewer joints than it uses,
both with a warning once: it draws in its rest pose, unskinned. When that happens with a skinned-only
custom material, the draw uses the default material. Outside a `BeginScene`/`EndScene` pair the call
does nothing.

### Custom skinned shaders

The engine's lit shader, `Renderer3D_Lit.glsl`, is built twice from one file: the skinned variant adds
`DE_SKINNED` to its vertex stage only, so the lit fragment code exists once. A custom shader gets the
same switch, `ShaderParams::AddDefine`, which applies to every stage, is part of the bytecode cache
key and survives `Shader::Reload`:

```cpp
Shader* shader = Shader::Create(ShaderParams()
    .SetName("Ghost")
    .SetFilePath("shaders/ghost.glsl")
    .AddDefine("DE_SKINNED"));
Material* ghost = Material::Create(MaterialParams().SetShader(shader).SetCullMode(CullMode::None));
```

A skinned vertex stage differs from a static one in two ways. Copy it from `Renderer3D_Lit.glsl`:

1. **The vertex inputs** are `a_Position` (location 0), `a_Normal` (1), `a_TexCoord` (2), `a_Joints`
   (`uvec4`, 3) and `a_Weights` (`vec4`, 4). Use these names: D3D11 and D3D12 match inputs by name,
   Vulkan by location. There is no `a_Color`; the colour comes from the block below. Positions are in
   skin space, so the shader skins them and applies `Model` itself.
2. **A uniform block named `SkinData`**, in std140, exactly this, at a binding from 2 to
   `Renderer3D::k_MaxSkinDataBinding` (13, because D3D11 has 14 constant buffer slots) that the
   material's own textures and samplers leave free:

   ```glsl
   layout(std140, binding = 6) uniform SkinData   // any free binding from 2 to 13
   {
       mat4 Model;          // the transform passed to SubmitSkinnedMesh
       mat4 NormalMatrix;
       vec4 Color;
       mat4 Joints[128];    // model space: RootTransform * global * inverseBind
   };
   ```

   Textures sit at binding `2 + 2i` and samplers at `3 + 2i` for slot `i` (see
   [Custom materials](scenes-and-ecs.md#custom-materials-per-mesh-shaders)), so a material with
   texture 0 and sampler 0 set occupies 2 and 3, and leaves 4 and up free.

The renderer finds the block **by name**, through `Shader::FindUniformBufferBinding("SkinData")`,
which needs a reflected shader (`ShaderParams::Reflect` is true by default) and binds its buffer there
with `Material::SetSkinUniformBuffer`. The rest of the binding convention (0 scene, 1 material
uniforms, 2+ textures) is unchanged, and static batches bind no skin buffer.

When the block is missing, is outside 2 to 13, or collides with a bound texture or sampler, the mesh
draws with the **default lit material** and a warning (once), instead of vanishing.

### Statistics

`Renderer3D::GetStatistics()` is reset at each `BeginScene` and complete after `EndScene`:

| Field | Meaning |
|---|---|
| `SkinnedDraws` | Skinned meshes drawn. Also counted in `DrawCalls` and `SubmittedMeshes`, but not in `VertexCount` or `IndexCount`, which describe the batches. |
| `SkinnedInstances` | Joint palettes uploaded. A model's submeshes share one. |
| `DroppedSkinnedDraws` | Skinned meshes of instances dropped by the per-frame budget. |
| `SkinnedJoints` | Joint matrices uploaded. |

The F4 Renderer tab shows the same numbers ([Debugging](#debugging)).

## The animator

### A standalone Animator

`Animator` needs no scene. It plays clips on a `Skeleton`, keeps the pose, and collects events. It
does not own the skeleton, which the `Model` does.

```cpp
Animator animator(model->GetSkeleton());
const AnimationClip* walk = model->FindAnimation("Walk");
const AnimationClip* jump = model->FindAnimation("Jump");

animator.Play(walk, 0.25f);                                    // fade in over a quarter second
animator.Play(AnimationState::Clip(jump).SetLoop(false).SetSpeed(1.5f));

animator.Update(dt);                                           // advance, pose, collect events
const std::span<const glm::mat4> palette = animator.GetSkinningPalette();
```

An `Animator` constructed without a skeleton, or given a null one, plays nothing. `SetSkeleton(s)`
binds another skeleton and returns to its rest pose with nothing playing; layer settings and
parameters stay, and the ranges that were open end at the next `Update` ([Ranges](#ranges)). An
`Animator` can be copied.

**What it plays.** `Play(state, fadeSeconds = 0, layer = 0)` takes an `AnimationState`; `Play(clip,
...)` is shorthand for `AnimationState::Clip(clip)`.

| `AnimationState` | Meaning |
|---|---|
| `AnimationState::Clip(clip)` | One clip. A null clip is what shows without it: the rest pose on layer 0, the layers below on any other. |
| `AnimationState::Blend1D(parameter, points)` | Clips blended along a float parameter ([Blending](#blending)). |
| `.SetLoop(bool)` | Loops by default. A clip that does not loop holds its last frame. |
| `.SetSpeed(float)` | A multiple of the clip's own pace, 1 by default. A negative speed plays it backwards, from the end. |

**Play rules.**

- `Play` fades the new state in over whatever shows now, linearly, in `fadeSeconds`. 0 cuts to it. A
  `Play` in the middle of a fade blends on from the mix, so nothing pops. With a fade and nothing
  playing on layer 0, it fades from the current pose.
- **Playing the state that already plays does nothing**, even once a clip that does not loop has
  finished, so a script may call `Play` every frame. With a fade of 0 it finishes that state's
  fade-in instead. `SetTime(0)` restarts. "The same state" means the same clip or blend with the same
  loop and speed: a different `SetSpeed` is a different state and starts from the beginning, so change
  the pace of a playing clip with `AnimatorComponent::Speed` (or by scaling the `dt` you give a
  standalone `Update`), not by replaying it with another speed.
- `Stop(fadeSeconds = 0, layer = 0)` fades back to what shows without the layer: the rest pose on
  layer 0, the layers below on others.
- **Cross-fades and the 4-state cap.** A layer mixes a stack of at most **4** states, each fading in
  over the ones below. When one reaches full weight everything under it is dropped. Blending between
  states lerps translation and scale and takes the shortest-path normalised lerp of rotations. A fifth
  state while four are still fading freezes the mix so far into one held pose (it shows in
  `GetStates` as `Frozen`) and fades in over that, so a burst of `Play` calls never pops. A layer at
  weight 0 freezes the mix its states have reached while hidden, not the last pose it showed.

**Reading it.** Every query but `GetFloat` takes a layer, 0 by default.

| Call | Gives |
|---|---|
| `GetCurrentClip(layer)` | The clip of the state the latest `Play` started; for a blend the clip with the larger share. Older states may still be fading out under it. |
| `GetTime(layer)`, `GetNormalizedTime(layer)` | Seconds into the current clip (or blend cycle), and 0 to 1 through it. |
| `SetTime(seconds, layer)`, `SetNormalizedTime(fraction, layer)` | Seek the current state. The pose follows on the next `Update` or `Evaluate`. |
| `IsFinished(layer)` | A state that does not loop has reached its end (or its start, when time last ran backwards). |
| `IsFading(layer)` | A cross-fade is in progress. |
| `GetStates(layer)` | What the layer is mixing, oldest first, as `AnimatorStateInfo` (below). |
| `GetFloat(name)` | A parameter's value. Unset parameters read 0. |

`GetStates` reports each `AnimatorStateInfo`: `Clip` (a blend's heavier clip; null for a frozen state
and for one that shows what lies below), `Parameter` (a blend's parameter, valid until the animator
next changes), `Blend`, `Frozen`, `Looping`, `Weight` (how far it has faded in, 0 to 1), `Time`
(seconds, or a blend's phase) and `NormalizedTime`. It exists for tooling such as the
[F7 tab](#debugging).

**Time and the pose.**

- `Update(dt)` advances every playing state by `dt`, poses the skeleton and collects the frame's
  [events](#events).
- `Evaluate()` poses the skeleton from where the states stand **without** moving time or firing
  events. Use it after a seek.
- `GetLocalPoses()` is one `JointPose` per joint (the rest pose until the first `Update`),
  `GetGlobalTransforms()` is the joints relative to the skeleton's root transform, and
  `GetSkinningPalette()` is one matrix per **skin** joint, ready for `SubmitSkinnedMesh`.
  `GetJointTransform(joint)` is `RootTransform * global`: the joint in the model's space (identity for
  an invalid index).

### AnimatorComponent

An `AnimatorComponent` next to a `SkinnedMeshRendererComponent` gives the entity an `Animator` for its
model. The component holds settings only.

| Field | Default | Meaning |
|---|---|---|
| `std::string DefaultClip` | empty | The clip played, looping, when the animator is created or its model changes. Looked up with `Model::FindAnimation`; a name the model lacks warns and plays nothing. |
| `bool PlayOnStart` | true | `false` leaves the rest pose until you `Play`. |
| `float Speed` | 1 | Multiplies the delta time given to the animator. |
| `bool Enabled` | true | `false` holds the pose: no update, no events. |

```cpp
entity.AddComponent<AnimatorComponent>(AnimatorComponent("Walk")).Speed = 2.0f;   // plays at double pace

Animator* animator = scene.GetAnimator(entity);        // for playing clips from a script
animator->Play(model->FindAnimation("Run"), 0.25f);
```

`Scene::GetAnimator(entity)` returns the entity's animator, creating it on first use. It is null
unless the entity has an `AnimatorComponent` and a `SkinnedMeshRendererComponent` whose `Model` has a
skeleton.

**Where it runs.** `Scene::OnUpdate(dt)` goes: scripts, then animation, then physics, then audio.
The animate pass advances every enabled animator by `dt * Speed`, and then delivers its
[events](#events) to the entity's script. It runs before physics so that a kinematic body on a
[socket](#sockets) follows this frame's pose. The delta is capped at 4/60 s, for scripts, animation
and physics alike, so a stall runs the scene slow instead of skipping through clips.

**Lifetime.**

- The animator is created by the first `GetAnimator` or update, and posed at once (`Evaluate`), so a
  disabled animator and a body baked on one of its joints show `DefaultClip`'s first frame. The
  `Animator*` stays valid while both components live.
- It survives `Scene::OnStop`/`OnStart` (animation does not depend on a physics world), and is freed
  with the entity or with its `AnimatorComponent`.
- When the `Model` changes to one with another skeleton, the animator is rebound: back to
  `DefaultClip`, posed at once. Its layer settings and parameters stay. The pointer stays valid.
- `Enabled = false` is not updated by the scene. Call `animator->Update(0.0f)` or `Evaluate()`
  yourself to show a change made to a paused animator.
- `DuplicateEntity` copies the component; the copy's animator starts from the beginning.

## Blending

### Blend by a parameter

`AnimationState::Blend1D(parameter, points)` blends the two clips around the value of a float
parameter. Set the parameter with `animator.SetFloat(name, value)` as often as you like; it needs no
`Play`.

```cpp
const AnimationState locomotion = AnimationState::Blend1D("Speed", { { 0.0f, idle }, { 1.5f, walk }, { 4.0f, run } });
animator.Play(locomotion, 0.2f);
animator.SetFloat("Speed", glm::length(velocity));
```

- Points are sorted by `Value`. Between two the blend is linear in the parameter; past either end the
  end clip plays alone.
- **The clips run in step.** A blend keeps one **phase** from 0 to 1 that every clip in it shares,
  each sampled at the same fraction of its own cycle. The phase advances at the **blended cycle
  length**, which lerps the two neighbouring clips' durations by the same weight. A walk turning into
  a run therefore never puts a foot down twice, provided the clips are authored in step (the same foot
  down at the same fraction of the cycle).
- A blend's `GetTime()` is phase times cycle length, `SetNormalizedTime` seeks the phase, and
  `GetCurrentClip` reports the clip with the larger share. A side with zero length (no clip, or a
  one-key pose) takes the other side's cycle rather than lerping towards 0.
- The parameters belong to the animator and are shared by every layer and blend; the same
  `Play(blend)` again is a no-op, so call it every frame if that suits your script.

See `examples/Marionette` (`Locomotion`): idle, walk and run blend on one `Move` parameter, and
`step_l` / `step_r` sit at one shared fraction of the walk's and the run's cycle, so a footstep sound
fires once per step at any speed.

### Layers

Layer 0 always poses the whole body at full weight. A higher layer **overrides** the joints in its
mask, on top of the layers below it, at its weight.

```cpp
animator->SetLayer(1, AnimationLayer().SetMask("b_Spine01_02").Exclude("b_Neck_04").SetWeight(1.0f));
animator->Play(wave, 0.15f, /*layer*/ 1);                 // a wave on the upper body
animator->SetLayerWeight(1, 0.5f);                        // fade the whole layer
animator->Stop(0.2f, 1);                                  // let the layers below show again
```

| `AnimationLayer` | Meaning |
|---|---|
| `SetMask(rootJoint)` | The subtree under that joint, the joint itself included. Empty (the default) is the whole skeleton. |
| `Exclude(joint)` | Leaves that joint's subtree out of the mask. Call it more than once. |
| `SetWeight(w)` | 0 shows the layers below, 1 replaces them inside the mask. |

- Each layer plays its own stack of states: `Play(state, fade, layer)`, `Stop(fade, layer)`,
  `PlayOneShot(..., layer)`, and every query take a layer index. `Play` on a layer that does not exist
  yet makes it, over the whole body.
- **Layers sample from the pose underneath**, so a joint the layer's clip does not animate keeps the
  pose below it, instead of snapping to the rest pose. A head-only clip on an upper-body mask turns
  the head and leaves the rest of the mask on the walk.
- A null clip on a layer above 0 is **transparent**: the pose below shows. That is how a layer fades
  in from nothing and how `Stop` fades out.
- Layers apply in index order. `SetLayer(0, ...)` is ignored. `GetLayerCount()` and `GetLayer(i)`
  read them back.
- A mask root or exclusion that the skeleton lacks warns when the layer is set. A missing root leaves
  the mask empty, so the layer moves nothing.
- **Additive layers are not supported.**

See `examples/Marionette` (`Fighter`): blocking is layer 1 masked from `spine` at weight 1, so a
fighter keeps walking while the arms guard. `Melee_Block` plays with `SetLoop(false)` so its `parry`
mark at the start of the clip fires, and the layer switches to the looping `Melee_Blocking` on
`IsFinished(1)`. A blocked hit is a one-shot on layer 1, which returns to the guard.

### One-shots

`PlayOneShot(clip, fadeIn = 0.1, fadeOut = 0.2, layer = 0)` plays a clip once over what the layer
plays, then fades back to it as the clip ends: a slash over a walk, a hit reaction over anything.

```cpp
animator->PlayOneShot(slash, 0.1f, 0.2f);
if (animator->IsOneShotPlaying())
    LockMovement();
```

- **The way back.** The interrupted state is kept, with its **time still running**, so a walk comes
  back in step. The fade back starts `fadeOut` seconds before the one-shot ends. If nothing was
  playing, the way back is transparent: the rest pose on layer 0, the layers below on others.
- `IsOneShotPlaying(layer)` is true from the call until the fade back starts.
- **A script that plays the interrupted state every frame does not cut the one-shot short.** A `Play`
  of that same state is a no-op while the one-shot is pending. A `Play` of anything else **cancels the
  way back**: the new state simply plays. `Stop` always cancels it, even over a layer that was empty.
- Another `PlayOneShot` restarts it and keeps the first's way back. A null clip does nothing.
- A one-shot does not loop, so its [events](#events) catch up from its start.
- **Marks near the end do not fire.** The way back takes the events over when it starts, `fadeOut`
  seconds before the clip ends, so a `done` mark in that last stretch is lost. Put end marks before
  `duration − fadeOut`, or pass a `fadeOut` of 0. A one-shot shorter than `fadeOut` plus half its
  `fadeIn` never leads, so none of its marks fire.
- **A one-shot always returns**, so it cannot hold a last frame. To stay down after a death, `Play` a
  state that does not loop (`AnimationState::Clip(death).SetLoop(false)`), as Marionette does.

## Events

A clip carries named marks on its timeline: an **instant** (a footstep) or a **range** (a sword's
hitbox, open from its start to its end). The animator reports each mark as playback crosses it.
Gameplay reads the timing from the animation instead of keeping its own timers.

### Authoring

In code, on a clip, with times in seconds into the clip:

```cpp
AnimationClip* slash = model->FindAnimation("Slash");
slash->AddEvent(0.12f, "whoosh");
slash->AddEventRange(0.32f, 0.48f, "hitbox");     // a range whose end comes first is swapped
slash->ClearEvents();                             // drop them all
```

An event outside `[0, duration]` never fires. `GetEvents()` lists the events as authored, and
`GetEventMarks()` the sorted instants, range starts and range ends the animator walks (at one time an
end comes before an instant, and an instant before a start; a range of zero length opens and closes
among the instants).

**The `.events` sidecar.** Timings live best beside the art. `Model::LoadFromFile` reads
`<model stem>.events` next to any model with clips (`Fox.gltf` reads `Fox.events`) and adds its events
to the clips. `Model::LoadEvents(path)` adds those of any other file, to the events already there, and
returns false if it cannot read the file.

```
# clip    time         event
Walk      0.287        step_fl
Walk      0.602        step_fr
Slash     0.32..0.48   hitbox
```

- One event per line: `<clip> <seconds> <event>` for an instant, `<clip> <begin>..<end> <event>` for
  a range. Whitespace separates them, `#` starts a comment (also after an event), and a clip or event
  name with spaces goes in double quotes (`"Hard Hit" 0.1 "impact flash"`).
- A byte order mark and CRLF line endings are fine.
- A line that does not parse, names a clip the model lacks, has an unclosed quote, or has a range
  that ends before it begins warns with the file name and line number and is skipped. A time that
  reaches past its clip's duration loads with a warning that part of it never fires.
- The Fox ships `Fox.events` with its real footfalls: each foot's lowest point, plus a range in
  `Survey`.

### Receiving events

In a scene, override `ScriptableEntity::OnAnimationEvent` on the entity's script:

```cpp
class Fighter : public ScriptableEntity
{
protected:
    void OnAnimationEvent(const AnimationEvent& event) override
    {
        if (event.Name == "hitbox")
            m_Swinging = event.Type == AnimationEventType::RangeBegin;
        else if (event.Name == "step_fl" || event.Name == "step_fr")
            PlayFootstep();
    }

private:
    bool m_Swinging = false;
};
```

An `AnimationEvent` has `Name` (a `std::string_view`), `Time` (where the mark sits on the clip, in
seconds), `Type` (`Instant`, `RangeBegin` or `RangeEnd`), `Clip` and `Layer`. `Clip` is null only for
the `RangeEnd` of a range that was open when the animator was bound to another skeleton.

- **When.** `OnAnimationEvent` runs in the animate pass: after every script's `OnUpdate` and before
  physics, once per event, in playback order. A `DestroyEntity` from the handler waits for the end of
  the pass, like one from `OnUpdate`. Every event is copied before any handler runs, so a handler may
  play, seek or destroy anything. An entity spawned by a script this frame hears its first events next
  frame, once its own script has started.
- **Polling.** Without a script, or from another one, read the animator
  (`Scene::GetAnimator(entity)`):

  ```cpp
  if (animator->IsEventActive("hitbox"))              // a range that has begun and not yet ended, on any layer
      TestHits();

  for (const AnimationEvent& event : animator->GetEventsThisFrame()) { /* the last Update's events */ }
  animator->ForEachEventThisFrame([&](const AnimationEvent& event) { /* must not Update the animator */ });
  ```

  The list holds the last `Update`'s events until the next one. A script's `OnUpdate` runs *before*
  the animate pass, so it sees the previous frame's events and ranges. The animate pass empties the
  list of an animator whose `AnimatorComponent` is disabled (`ClearEventsThisFrame`), so it reports
  no events while its open ranges stay active.

### Which clip fires

Only each layer's **dominant** contribution fires. The rule keeps blends and cancelled moves honest:

| Situation | The clip that fires |
|---|---|
| A `Blend1D` | The side with the larger share. |
| A cross-fade | The state with the largest share of the mix. An incoming state takes over once its fade weight passes 0.5; ties go to the newer state. |
| A one-shot | The one-shot, until its fade back starts; then the interrupted state takes over at once, after the one-shot's own marks for that frame. |
| A layer above 0 | Only at weight 0.5 or more. |

What that guarantees:

- A walk and a run blended 50/50 never double a footstep. Author both gaits' footsteps at the same
  **fraction** of their cycles (they advance in step), and a sweep from one gait to the other fires
  every footfall exactly once.
- A swing cancelled before it showed never opens its hitbox.
- A cross-fade from one looping clip to another never repeats footsteps: a looping clip does not catch
  up from its start when it takes over. A clip that **does not** loop (a one-shot) does, so its
  fade-in does not swallow its first marks.

### Crossing rules

- A mark fires when playback crosses it, after the previous update's time and up to this one's. A
  loop's wrap counts: a step that wraps fires from the old time to the clip's end, then from its
  start to the new time. A step of more than one lap fires a mark at most twice (a scene caps its
  step at 4/60 s). A state's start counts on its first `Update`, so a mark at time 0 fires. A mark
  at a one-shot's very end fires when its `fadeOut` is 0 ([One-shots](#one-shots)).
- Played backwards, a range opens at its end and closes at its start.
- A seek (`SetTime`, `SetNormalizedTime`) counts the new time as a start, so `SetTime(0)` replays a
  swing in full, and seeking into the middle of a range opens nothing. When the seeked state is the
  one firing the layer's events, its open ranges close at the next `Update`. `Evaluate()` poses the
  animator after a seek without firing anything.

### Ranges

A range that opened gets its `RangeEnd`:

- At its end time as playback crosses it.
- **Early**, when its clip stops being dominant (a cancelled swing still closes its hitbox).
- When the clip's events change under it (`ClearEvents`, or a [model reload](#hot-reload) that drops
  or renames it): it ends at the next `Update`, with the name it began with.
- When the entity's `AnimatorComponent` is removed: its script hears the `RangeEnd` in the next
  animate pass (`Animator::GetOpenRangeEnds` gives the same events to code that drops an animator).
- When the animator is bound to another skeleton (`SetSkeleton`, which a scene does when the entity's
  model changes to one with other joints): it ends at the next `Update`, with a null `Clip`, since
  the old model may be gone.

`IsEventActive("hitbox")` is true between the begin and the end. A range of zero length opens and
closes in one go. An end that arrives with no begin fires nothing.

### Names

`AnimationEvent::Name` stays valid for the life of the program. The engine keeps one copy of each name
ever authored, so a name you hold outlives the clip's events changing and a model reload.

### Combat windows from events

Ranges are enough to author a fighting game's timing, and `examples/Marionette` keeps none of it in
code. Its `.events` files give every move a `windup` (the telegraph the AI reads), a `hitbox` (the
active frames), a `combo` window (where a second press chains) and, for a dodge, `dash` and `iframes`:

```
# examples/Marionette/assets/animations/Rig_Medium_CombatMelee.events
Melee_1H_Attack_Slice_Diagonal   0.00..0.37   windup
Melee_1H_Attack_Slice_Diagonal   0.37..0.47   hitbox
Melee_1H_Attack_Slice_Diagonal   0.47..0.75   combo
```

The game polls the ranges with `IsEventActive` (`Fighter::IsWindowActive`) and takes the instants,
such as footsteps, in `Fighter::OnAnimationEvent`; a hit counts only while a `hitbox` is open. What
building it showed:

- **End every window before `duration − fadeOut`** of the one-shot that plays the move, or it never
  fires ([One-shots](#one-shots)). Marionette checks every move at load (`ValidateMoveset`) and
  warns, and again after each live edit.
- **Gate on your own state as well as on the range.** A cancelled move's `hitbox` stays open until
  its replacement takes over, at fade weight 0.5. Marionette counts a hit only while the attacker is
  still in its attack state.
- **A mark at the start of a clip needs a state that catches up.** A looping state that takes over
  does not fire a mark at time 0, so the `parry` window at the start of a block raise plays on a
  clip with `SetLoop(false)`.
- **A hit-stop slows the animator with `AnimatorComponent::Speed`.** Marionette chose it over
  `Enabled` because, before v0.8.3, a poller of a disabled animator read its last frame's events
  again and again ([#81](https://github.com/KingoBoiii/DingoEngine/issues/81)).
- Per-move numbers (damage, reach) live in a game-side table keyed by clip. An event carries a name,
  no payload ([#103](https://github.com/KingoBoiii/DingoEngine/issues/103)).

### Particles from events

*(v0.9)* A `ParticleEventComponent` turns events into particles with no game code: an instant event
bursts an emitter entity, and a range plays one while it is open. See
[Particles from animation events](particles.md#from-animation-events).

## Sockets

`child.SetParent(character, "joint", keepWorldTransform)` hangs an entity on a joint of the
character's skinned model. The entity's world transform becomes the character's world transform times
the joint's frame times its own local transform, using **this frame's pose**, so a sword follows the
hand through every swing. Everything that reads world values follows the joint: rendering, lights (a
lantern on a belt, a light on a staff), audio sources, and physics.

```cpp
Entity sword = scene.CreateEntity("Sword");
sword.AddComponent<Transform3DComponent>();                       // local: the offset in the hand joint's frame
sword.AddComponent<MeshRendererComponent>(MeshRendererComponent(swordMesh));
sword.SetParent(character, "b_RightHand_08", false);                // false: keep the local values

sword.GetParentJoint();                                           // "b_RightHand_08"
```

- **`keepWorldTransform`** works as for any parent ([Parenting](scenes-and-ecs.md#parenting-v071)):
  true (the default) rewrites the local transform so the entity stays where it is, relative to the
  joint's current pose; false keeps the local values as the offset in the **joint's frame**, whatever
  the rig's bone axes are (the Fox's head joint has x along the snout and y up through the crown).
  Setting the parent and joint the entity already has changes nothing; the same parent with another
  joint re-attaches it.
- **The joint passes on its position and rotation, not its scale**, so an FBX model's centimetre
  scale (its 0.01 root transform) does not shrink what it holds. The character entity's own
  `Transform3DComponent` scale still applies, as with any parent.
- `GetParentJoint()` returns the joint's name, or an empty string for an entity that follows its
  parent's origin. `SetParent(character)` without a joint attaches to the character's origin instead
  (with the default `keepWorldTransform` the entity stays where it is).
- **Unknown joints.** A joint name the parent's model lacks warns, and the child sits at the model's
  origin until the model has one of that name. Without an animator the joint stays at its rest pose.
- `DuplicateEntity` copies a socket with its entity, and destroying the character destroys the
  children hanging on it.

**Physics on a joint.** A body on a socket follows the parent's rules ([Physics under a
parent](scenes-and-ecs.md#parenting-v071)). A **kinematic** body is driven to this frame's pose before
physics steps (the animate pass runs first), so a hitbox follows the animation. Dynamic bodies are
simulated in world space and a static body's collider stays put, so use kinematic bodies for anything
that rides a joint.

**The ancestor collision filter.** A kinematic child's body ignores every ancestor's body, and an
ancestor's character controller passes through it, so a hitbox on a character's hand never shoves
the character carrying it. Nothing else is filtered: dynamic and static children, siblings and
unrelated bodies collide as before, and 2D physics has no such rule. The pairs are worked out at the
start of every 3D step, so `SetParent` and `RemoveParent` take effect on the next one.
`Physics3D::IsCollisionIgnored(a, b)` and `CharacterController3D::IsBodyIgnored(body)` report them.

Parent an entity before its body is built, as for any parent; what happens when you parent one that
already has a body is in [Physics under a parent](scenes-and-ecs.md#parenting-v071).

See `examples/Marionette` (`Fighter::SpawnWeapon`, `SpawnRigSphere`): each sword, axe and shield is
socketed to `handslot.r` or `handslot.l`, and the spheres its combat tests ride the weapons and the
body's joints. They are plain transforms, not bodies, and the hit test reads their world positions;
`--debug-hitbox` draws the same entities.

## Retargeting

A clip plays on any skeleton that has the joints it names. When the clip was loaded with **another
skeleton** (`clip->GetSourceSkeleton()` is not the animator's: a second character, a
[clip library](#clip-libraries), even a second load of the same file), the animator retargets it by
joint name:

- **Rotations come from the clip.** Scale never does.
- **Translation comes only for root-most moving joints**: those with no ancestor in the target whose
  translation the clip moves (usually the hips). A track moves when any key leaves the source joint's
  rest offset (by more than 1e-5 of its length); a track that only holds that offset, like the still
  `root` that exporters such as KayKit's key on every joint, doesn't take the hips' motion away. A track
  held at any other offset, or naming a joint the source skeleton lacks, counts as moving. The clip's
  motion there is mapped as `targetRest + (key - sourceRest) *
  ratio`, where `ratio` is the length of the target's rest offset of that joint from its parent divided
  by the source's (1 if either has no length). The keys live in the parent's frame, so units and root
  scale cancel: a rig in centimetres under a 0.01 root scale drives one in metres, and a character
  1.5 times as long moves its hips 1.5 times as far.
- **Every other joint keeps the target's own rest translation**, so the target's bone lengths stay its
  own, however the source was proportioned.
- Joints the clip does not name stay at rest. A channel naming a joint the target lacks is skipped. A
  clip that animates none of the target's joints warns the first time it is bound, and plays as the
  rest pose.

```cpp
Model* library = Model::LoadFromFile("models/Moves.fbx");     // clips only
Model* heavy   = Model::LoadFromFile("models/Heavy.fbx");     // the same rig, bigger

Animator animator(heavy->GetSkeleton());
animator.Play(library->FindAnimation("Slash"), 0.1f);                  // retargeted by name
```

**Requirements.** The rigs need the **same joint names and the same rest orientations**: one rig
template at different proportions. Retargeting across different topologies is not supported. A clip
retargeted onto an identical rig gives the same pose as playing it natively when its keys are
rotations, plus translation on the root-most moving joints: other translation and all scale keys are
dropped.

Clips keep their events when they retarget, because events live on the clip.

See `examples/Marionette` (`GameAssets`, `Moveset`, `--check`): five clip libraries on KayKit's
`Rig_Medium` (23 joints, the same names and rest rotations in every file) play on the Knight, the
Barbarian and two skeletons. The four share one body, so the ratio is 1 and the retarget is exact;
the opponents differ in weapon, pace and uniform scale, not limb length. `--check` compares a
retargeted idle with the library's own pose joint by joint.

## Hot-reload

With the AssetManager's hot-reload on (`params.Assets.EnableHotReload = true`, or the toggle in the F6
tab), saving a model's file, or its `.events` file, reloads it **in place**, while the game runs. For
an unmanaged model call `Model::Reload()`; for a managed one `AssetManager::Reload(handle)`.

- Every pointer a game may hold survives: the `Model*`, each `Mesh*` (with a new `GetId()`), each
  `Material*`, each `Texture*` and each `AnimationClip*` (matched by name, with the new keys and
  events). The `Skeleton*` survives while every joint keeps its name and parent. An `Animator` playing
  a clip keeps playing it at the same time, now on the new keys.
- **If the joints changed**, a new `Skeleton` replaces the old one, which stays alive for animators
  still bound to it. A scene's animators start over on the new skeleton with their `DefaultClip`. A
  standalone `Animator` keeps running on the old one until you `SetSkeleton`.
- A save of the `.events` file alone only replaces the clips' events (`Model::ReloadEvents()`).
- A file that no longer loads logs an error and leaves the model as it was.
- `Model::GetGeneration()` counts the successful reloads, `ReloadEvents` included.
- **Events you added in code with `AddEvent` are dropped by a reload**, replaced by the file's and the
  `.events` file's. Prefer the sidecar for anything that should survive, or add them again when the
  generation moves:

  ```cpp
  if (model->GetGeneration() != m_SeenGeneration)
  {
      m_SeenGeneration = model->GetGeneration();
      model->FindAnimation("Slash")->AddEventRange(0.32f, 0.48f, "hitbox");
  }
  ```
- Physics bodies already built from a mesh keep their shape until they are rebuilt (the next
  `OnStart`). A body made after the reload gets the new one.

The table of what each object does through a reload, the watched files and the `.bin` caveat are in
[Model hot-reload](asset-pipeline.md#model-hot-reload-v08). To watch it work, run the Animation Test
with `--anim=clip --anim-reload` ([below](#debugging)).

`AnimationClip::GetEventRevision()` changes whenever a clip's event list does (a reload,
`AddEvent`, `ClearEvents`) and never goes back, so a game can tell when to re-check what it derived
from the events. See `examples/Marionette --live-edit-demo` (`GameAssets::PollEventChanges`): it
plays a copy of its assets with hot-reload on, rewrites the slash's `hitbox` range in the copy's
`.events` file after ten seconds, and the next swing lands at the new time.

## Debugging

**F7, the Animation tab** of the Debug window (`UI::AnimationSection()` embeds it in your own window,
`UI::AnimationStatsWindow()` opens it as a window of its own, and `UI::DebugTab::Animation` selects
it). It lists every live scene's
animators, once they have been created:

- the entity, scene and model, with `disabled` and the speed when they differ from the defaults;
- per layer: its weight, its mask (or "whole body") and a `one-shot` flag;
- per state: a label (the clip; `Blend1D Speed = 1.20 [Walk]` for a blend with its parameter and
  heavier clip; `(frozen)`; `(rest pose)` on layer 0 or `(below)` above it; `(once)` when it does not
  loop), a fade-weight bar, and a time bar (seconds over duration, or a blend's phase);
- below that, the **last 20 events** of all scenes, newest first, with entity, clip, event, type,
  layer and time. The scene records them every update, with the tab closed.

Past 16 animators the list scrolls in its own region, so the events stay in view.

**F4, the Renderer tab** shows the skinned draws, an Instances bar against the
`MaxSkinnedInstances` budget, the joints uploaded, and in red the dropped draws when the budget is
exceeded.

**`Animator::GetStates(layer)`** is the same view for your own tooling.

**The Animation Test** in the test app: `--test=anim`, run with the test app's source directory
(`test/`) as the working directory so its `assets/` path resolves. `--anim=` picks the scene (the
default is `bind`; an unknown value warns and shows `bind`), and every check logs `[PASS]` or
`[FAIL]` and shows in the Properties panel.

| `--anim=` | Shows |
|---|---|
| `bind` | The Fox through the skinned path at the rest palette: one skinned draw of 24 joints. |
| `bindstatic` | The same Fox through `MeshRendererComponent` (the batched path), to compare. |
| `pose` | A hand-posed Fox drawn with `SubmitSkinnedMesh`, no scene entity. |
| `clip` | A Fox played by an `AnimatorComponent`, with a box on its `b_Head_05` socket. |
| `blend` | A Speed slider blending Survey (0), Walk (1.5) and Run (4). |
| `layers` | Walk with an upper-body Survey layer from `b_Spine01_02`, a weight slider and a one-shot Run button. |
| `events` | The Fox walking the Speed blend, with a footprint dropped under each foot as its step event fires, and an event log. |
| `crowd` | Many foxes against the instance budget, with timing. |

| Flag | Applies to | Effect |
|---|---|---|
| `--anim-clip=Survey`, `Walk` or `Run` | `clip`, `layers`, `crowd` | The clip to play (`clip` and `layers` default to Walk; `crowd` animates every fox, out of step). |
| `--anim-time=S` | `clip`, `layers` | Freeze at S seconds. |
| `--anim-speed=X` | `blend` | The Speed parameter. |
| `--anim-phase=F` | `blend` | Freeze at that fraction of the cycle. |
| `--anim-count=N` | `crowd` | Crowd size (default 64). 65 shows the budget dropping one. |
| `--anim-static` | `crowd` | Draw the crowd through `MeshRendererComponent`, for comparison. |
| `--anim-skeleton` | all but `crowd` and `pose` | Draw every joint and bone as boxes inside a see-through Fox (also a checkbox). |
| `--anim-reload` | `clip` | Play a managed copy of the Fox with hot-reload on and edit it on disk: after 1 s Walk and Run trade names, then `Fox.events` gains a mark. Each reload is a check. |
| `--no-vsync` | test app | Time the crowd. |

## Limits

- **128 joints a draw.** A mesh whose vertices name more than `Renderer3D::k_MaxSkinJoints` (128)
  joints draws its rest pose, unskinned, with a warning. Joints a skin cannot reach (helpers, ends,
  animated cameras) sort after the skin joints and do not count.
- **`MaxSkinnedInstances` a frame** (64 by default, at most 256), across all scenes of a renderer. The
  rest are dropped whole, with a warning.
- **4 states a layer.** A fifth freezes the mix so far into one pose.
- **Translucent characters sort per mesh**, by the centre of each mesh's rest bounds, so a
  character's own see-through parts can blend in the wrong order with each other.
- **Colliders follow the rest pose.** Bodies and mesh colliders do not deform with the animation.
- **Retargeting is by name** within one rig template. Different topologies, different joint names or
  different rest orientations need the rig to be fixed in the art.
- **Models load on the main thread**, one asset a frame when loaded asynchronously.
- **Embedded textures are skipped.** An image embedded in a GLB gives its submesh no
  `DiffuseTexture`, and nothing is logged. Keep the PNG beside the file and put it in a lit material,
  as Marionette does for its characters ([#99](https://github.com/KingoBoiii/DingoEngine/issues/99)).

Not in v0.8:

| Item | Where it lives |
|---|---|
| Root motion | A v0.8.x stretch, [#104](https://github.com/KingoBoiii/DingoEngine/issues/104). Use in-place clips and move the body yourself. Moving it by a clip's travel while the pose also moves the hips counts the travel twice: Marionette instead pays each move's net hips travel over its fade-out, and moves a dodge over its `dash` range. |
| IK (foot, look-at) | v0.9 or later, or a module. |
| Additive layers, state-machine graph assets | Later. Game code drives `Play`. |
| Morph targets (blend shapes) | Later. |
| Cross-topology retargeting | Later. |
| Instantiating a static multi-node model as an entity tree | Later. Static models stay pre-transformed. |
| Inherited visibility (`SetActive`) | The v1.0 API pass. |
| **Skinned shadows** | Shipped in v0.9: skinned meshes cast through the shadow pass's own `DE_SKINNED` vertex stage (`Renderer3D_Shadow.glsl`, sharing `Skinning.glsl` with the lit shader). See [Shadows](shadows.md). |
| Animation LOD, culling and compression; instancing skinned meshes | v1.0 throughput work. |
| Worker-thread model parsing | Later. |

## See also

- [Scenes & ECS](scenes-and-ecs.md) - the components, `OnUpdate` order, parenting and
  [physics under a parent](scenes-and-ecs.md#parenting-v071).
- [Asset Pipeline](asset-pipeline.md) - loading models, the `.events` sidecar, and
  [Model hot-reload](asset-pipeline.md#model-hot-reload-v08).
- [Lighting & Shading](lighting.md) - lit materials, which skinned meshes draw with.
- [3D Physics](physics-3d.md) - kinematic bodies and character controllers.
- The test app's **Animation Test** (`test/`, `--test=anim`, flags [above](#debugging)).
- `examples/Marionette` - the worked example: combat windows from `.events`, a masked block layer,
  weapons and hit spheres on sockets, clip libraries shared by four characters, and live `.events`
  editing.

---

Back to the [documentation index](README.md).
