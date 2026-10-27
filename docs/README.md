# DingoEngine Documentation

DingoEngine is a C++20 game engine on [NVRHI](https://github.com/NVIDIAGameWorks/nvrhi),
running on Vulkan, DirectX 11 and DirectX 12 on Windows and Linux. It has a layered application
architecture, a batched 2D renderer, a lit 3D renderer with shadows, post-processing, particles
and skeletal animation, a scene/entity-component system with `ScriptableEntity` behaviours, 2D
and 3D rigid-body physics, audio, an asset manager with hot-reload and packed assets, and saves
and settings. The ECS is [EnTT](https://github.com/skypjack/entt), 2D physics is
[Box2D](https://box2d.org/), 3D physics is [Jolt](https://github.com/jrouwe/JoltPhysics) and
audio is [miniaudio](https://miniaud.io/), but those are internal details: client code never
includes or links any of them.

These guides cover how to **use** the engine from your own game code, whether you
link against a prebuilt release package (`.lib` + headers) or integrate the
engine from source.

## Guides

| Guide | What it covers |
|---|---|
| [Getting Started](getting-started.md) | Prerequisites, what's in a release package, project setup (include dirs, link libraries, defines), and a minimal window app. |
| [Application & Layers](application-and-layers.md) | The entry point, `Application` lifecycle, the `Layer` stack, input, and events. |
| [2D Rendering](rendering-2d.md) | `Renderer2D` (quads, circles, MSDF text), the camera/projection model, textures, and fonts. |
| [Scenes & ECS](scenes-and-ecs.md) | The v0.3 scene system — `Scene`, `Entity`, built-in components, `ScriptableEntity` behaviours, and `SceneManager`; plus (v0.4.1) 3D entities — `Transform3D`/`MeshRenderer`/`RigidBody3D` and the box/sphere/capsule/mesh colliders, drawn through `Renderer3D`; and the F9 Scene tab for inspecting live scenes, entities and components. |
| [2D Physics](physics-2d.md) | The v0.4 rigid-body system — `RigidBody2D`/`BoxCollider2D`/`CircleCollider2D` components, gravity, the physics lifecycle, and applying forces/impulses. |
| [3D Physics](physics-3d.md) | The Jolt-backed `Physics3D` — 3D rigid bodies with box/sphere/capsule colliders and (v0.6.2) triangle-mesh and convex-hull colliders, and constraints (fixed, point, hinge, swing-twist, distance, slider joints); usable standalone, or (v0.4.1) wired into the `Scene`/ECS. |
| [Asset Pipeline](asset-pipeline.md) | The v0.6 `AssetManager` — UUID asset handles, path dedup, background/async loading, and hot-reload of shaders, textures and (v0.8) models. |
| [Lighting & Shading](lighting.md) | The v0.7 lighting system — directional, point, spot and ambient lights (as components or through `Renderer3D`), the per-scene light budget, falloff, gameplay queries of a light's reach (`GetLightAttenuation`), lit materials with specular and emissive, the scene uniform block for custom shaders, and hot-reloading the lit shader. |
| [Shadows](shadows.md) | The v0.9 shadows: cascaded sun shadows, point and spot light shadows on a budget of slots, `ShadowCasting` per mesh (`ShadowsOnly` for cut-away walls), `Renderer3DShadowSettings`, skinned casters, gameplay shadow probes (`Scene::GetLightVisibility`), and receiving shadows in a custom shader. |
| [Post-processing](post-processing.md) | The v0.9 post chain: the HDR scene target, `PostProcessComponent` and `PostProcessStack`, the tone curves (`Soft`, `ACES`, `Neutral`, `None`) with exposure, bloom and ambient occlusion. |
| [Particles](particles.md) | The v0.9 GPU particles: `ParticleEffectParams`, emitters (`ParticleEmitterComponent`, `Renderer3D::CreateParticleEmitter`), bursts, soft particles, particles fired from animation events (`ParticleEventComponent`), and the F4 effect editor. |
| [Compute](compute.md) | The v0.9 compute shaders and storage buffers: `ComputePass`, `Renderer::Dispatch`, `GraphicsBuffer::CreateStorageBuffer`, storage images, and reading a storage buffer in a draw. |
| [Profiling](profiling.md) | The v0.9 GPU pass timers (`Renderer::BeginGpuTimer`) and the F8 Profiler tab, and Tracy behind premake's `--profile` option with the `DE_PROFILE_*` zone macros. |
| [Animation & Skinned Models](animation.md) | The v0.8 skeletal animation system — loading skinned models and clip libraries, GPU skinning (`SkinnedMeshRendererComponent`, `Renderer3D::SubmitSkinnedMesh`, the instance budget, custom skinned shaders), the `Animator` (cross-fades, `Blend1D`, masked layers, one-shots), timeline events and the `.events` sidecar, joint sockets, retargeting by joint name, in-place model hot-reload, and the F7 Animation tab. |
| [Game UI](game-ui.md) | The v1.0 DingoUI: immediate-mode roots with nine anchors and stretch, boxes with row/column layout, gap, padding, align and justify, sizing (fit, fixed, grow, percent, min/max), `Text` that wraps, `Image`, scaling with the screen, and one draw call per frame. |
| [Saves & Settings](saves-and-settings.md) | The v1.0 save system: `SaveData` (a typed tree written as JSON), atomic writes with a `.bak`, `SaveSlots` in the player's folder, and `Settings` (display, audio and input options) with the display-mode list. |
| [Audio](audio.md) | The miniaudio-backed `AudioEngine` — clips, `Play`/`PlayOneShot`, positional audio and distance falloff, and mix buses (volume, mute, pause and stop per bus, `AudioSourceComponent::Bus`). |
| [Asset Packages](distribution.md) | Asset packages: `<Game>.dpak`, the virtual file system the engine reads through, `embedPackKey()` and the DingoPack tool, `AssetManager::Package`/`Unpackage`, and what the encryption does and doesn't protect against. |

| [Performance](performance.md) | The v1.0 throughput work: frustum culling, automatic instancing, static batches, the frame budget, and how to read the F8 counters when a scene is slow. |
| [Shipping](shipping.md) | The checklist for handing a game to players: the build to ship, what goes in the folder, what the game writes on their machine, and crash reports. |

## Reference

| Page | What it covers |
|---|---|
| [API conventions](conventions.md) | Who owns and frees what, the shape of params structs, enums and handles, naming, and how the API reference is documented. |
| [Versioning](versioning.md) | What 1.x promises: Semantic Versioning, what counts as the public API, deprecation, and what may change in a minor release. |
| [Migrating to 1.0](migrating-to-1.0.md) | Every change from v0.9 that breaks a build or changes behaviour, with the code to write instead. |

The API reference is built from the headers' `///` lines with Doxygen (`doxygen docs/Doxyfile`,
see [conventions](conventions.md#documentation)), with these guides as its pages.

## A 30-second tour

```cpp
#include <DingoEngine/EntryPoint.h>   // include in exactly ONE .cpp — it defines main()
#include <DingoEngine.h>

using namespace Dingo;

class GameLayer : public Layer
{
public:
    GameLayer() : Layer("Game") {}

    void OnUpdate(float dt) override
    {
        Renderer2D& r = Application::Get().GetRenderer2D();
        r.BeginScene(m_Camera);
        r.Clear({ 0.1f, 0.1f, 0.12f, 1.0f });
        r.DrawQuad({ 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.9f, 0.3f, 0.3f, 1.0f });
        r.EndScene();
    }

private:
    glm::mat4 m_Camera = glm::ortho(-8.0f, 8.0f, -4.5f, 4.5f, -1.0f, 1.0f);
};

class Game : public Application
{
public:
    Game(const ApplicationParams& p) : Application(p) {}
    void OnInitialize() override { PushLayer(new GameLayer()); }
};

Application* Dingo::CreateApplication(ApplicationCommandLineArgs args)
{
    ApplicationParams params{ .CommandLineArgs = args };
    params.Window.Title = "My Game";
    return new Game(params);
}
```

See [Getting Started](getting-started.md) for the full setup, then
[Scenes & ECS](scenes-and-ecs.md) for the recommended way to structure a real game.

## Conventions used in these docs

- Everything lives in the `Dingo` namespace.
- World units, not pixels: positions and sizes you pass to `Renderer2D` are in the
  coordinate space defined by your camera matrix. A quad's *position is its center*.
- Resources (`Texture`, `Font`) are created with `Create*()` factories that return
  raw pointers; you own them and `delete` them when done ([conventions](conventions.md)). As of v0.6 the
  preferred route for file-backed assets is the [AssetManager](asset-pipeline.md),
  which owns what it loads and hands out stable handles instead.
