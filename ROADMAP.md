# DingoEngine Roadmap

## v0.1 — Core Foundation
Core components, windowing, input, and a basic rendering pipeline. ImGui for debugging UI.

## v0.2 — Extended Rendering Pipeline
Extended graphics and rendering pipeline, including DirectX 12 support as a second graphics back-end alongside Vulkan. The D3D12 implementation via NVRHI already exists in the codebase (`DirectX12GraphicsContext`) but is currently disabled — this milestone activates and stabilises it.

## v0.3 — Scenes & ECS
Scene management and entity component system (ECS) using the EnTT library.

## v0.4 — Physics & Collision
2D rigid body simulation, AABB/circle collision, and a physics world integrated into the ECS (e.g. `RigidBody2DComponent`, `BoxCollider2DComponent`). FlappyBird's manual collision logic can be replaced as a showcase.

## v0.5 — Audio System
Proper audio engine (OpenAL or miniaudio), `AudioSource` / `AudioListener` ECS components, and 3D positional audio. Replaces the hardcoded .ogg playback currently in the FlappyBird example.

## v0.6 — Asset Pipeline & Hot-Reload
Centralized `AssetManager` with UUID-based handles, background/async loading, and hot-reload of shaders and textures during development.

## v0.7 — Scripting
C# scripting via Mono or .NET CoreCLR, or Lua. Allows game logic to live outside the engine binary and be iterated on without recompiling.

## v0.8 — Editor
A standalone `DingoEditor` application built on ImGui: scene hierarchy panel, component inspector, asset browser, and a viewport with transform gizmos (ImGuizmo).

## v0.9 — Serialization
Scene save/load via YAML (yaml-cpp), a prefab system, and project files. Ties the editor to persistent, shareable scenes.

## v1.0 — Stability & Polish
Performance profiling integration (Optick or Tracy), full API documentation, cross-platform validation (Linux + Vulkan), and a complete example game shipped alongside the engine.
