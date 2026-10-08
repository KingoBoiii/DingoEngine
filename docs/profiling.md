# Profiling

*(v0.9)*

Two tools, and they work apart:

- **GPU pass timers** are always compiled in. They time the renderer's passes with GPU timer
  queries and show in the debug window's **F8 Profiler** tab, next to the main thread's frame split.
- **Tracy** records CPU zones on every engine thread, and the GPU timers as plots. It is compiled in
  only when you ask for it, so a normal build links no Tracy code.

## The F8 Profiler tab

| Section | What it shows |
|---|---|
| Tracy | Whether Tracy is compiled in, and whether the viewer is connected. |
| CPU | The main thread's last frame (`Application::GetFrameTimings`): the whole frame, the wait for the render thread inside `Renderer::BeginFrame`, the update (`AssetManager::Update` and every layer's `OnUpdate`, which is where the draws are recorded) and the UI. Then the render thread's time to execute the frame's command list and present it. Last, mean and max over the last 120 frames the tab was open for. |
| GPU passes | Every GPU timer, nested under the timers open around it when it first ran: `Frame` (the whole command list), `Renderer3D` (each `EndScene`), `Renderer2D` (each scene, `BeginScene` to `EndScene`) and `ImGui`. A timer that runs several times a frame (one per scene) adds up to one sample. |

GPU results are read four frames after they were recorded, so the reader never waits for the GPU.
A query that still isn't done by then is dropped, and a frame that renders nothing (a minimized
window) measures nothing.

## Timing your own passes

```cpp
Renderer::BeginGpuTimer("Minimap");
// ... draws ...
Renderer::EndGpuTimer();
```

Timers nest and must pair up within a frame; one left open is closed at `EndFrame`. At most 32 a
frame: later ones warn once and are skipped. `Renderer::GetGpuTimers()` returns the same statistics
the tab shows.

## Tracy

Regenerate the solution with the option, then build as usual:

```
./vendor/premake/bin/premake5.exe --profile vs2026
```

This compiles `vendor/tracy` (v0.11.1) into `DingoEngine.lib` with `TRACY_ON_DEMAND`: nothing is
collected or sent until a Tracy **v0.11.x** viewer connects, so a `--profile` build can run without
one. Every project in the workspace gets `DE_PROFILE`, so a game's own zones compile in too.

The engine records:

- the frame (`DE_PROFILE_FRAME`) and `Application::Run`'s steps on the thread named **Main**:
  `Renderer::BeginFrame`, `AssetManager::Update`, each layer's `OnUpdate` and `OnUIRender` (the
  layer's name is the zone's text), `ImGui`, `Renderer::EndFrame`;
- `Scene::OnUpdate` (the scene's name as text) and its systems: scripts, animation, physics, audio;
- `SceneRenderer::Render`, `Scene::RenderEntities` and `RenderEntities3D`, `Renderer3D::EndScene`
  with its light resolve, batches and skinned draws, and `Renderer2D::Flush`;
- the thread named **Render**: `Renderer::Execute` and `SwapChain::Present`;
- every GPU timer as a plot, `GPU <name> (ms)`.

### Zones in your own code

`DingoEngine/Core/Profiler.h` holds the macros. They compile to nothing without `--profile`.

```cpp
void EnemyDirector::Update(float dt)
{
    DE_PROFILE_FUNCTION();                 // a zone named after the function
    {
        DE_PROFILE_SCOPE("Pathfinding");   // a named zone until the end of the block
        ...
    }
    DE_PROFILE_SCOPE_TEXT("Spawn", wave.Name); // with text shown beside the zone
    DE_PROFILE_PLOT("Enemies alive", m_Enemies.size());
}
```

`DE_PROFILE_THREAD("Loader")` names the calling thread. A zone or plot name must be a string literal
(or a string that lives for the whole run): Tracy identifies them by address.

Tracy stays out of the public headers: `Profiler.h` declares the engine's own `ProfileSite` and
`ProfileZone`, and only `src/DingoEngine/Core/Profiler.cpp` includes Tracy.
