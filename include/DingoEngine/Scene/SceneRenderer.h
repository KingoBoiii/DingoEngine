#pragma once

namespace Dingo
{

	class Scene;
	class Renderer2D;
	class Renderer3D;

	// Renders a Scene by reading its primary CameraComponent (projection) and that
	// camera entity's transform (view), plus an optional DirectionalLightComponent,
	// then dispatching the 2D or 3D pass to Renderer2D / Renderer3D. Holds non-owning
	// references to the engine renderers (owned by Application). This is the single
	// per-scene render entry point — SceneManager::OnRender() delegates here.
	class SceneRenderer
	{
	public:
		SceneRenderer(Renderer2D& renderer2D, Renderer3D& renderer3D);

		SceneRenderer(const SceneRenderer&) = delete;
		SceneRenderer& operator=(const SceneRenderer&) = delete;

		// Renders the scene's renderable entities through its primary camera, clearing
		// to the scene's clear color. No-op (warns once) if the scene has no camera.
		void Render(Scene& scene);

	private:
		Renderer2D* m_Renderer2D = nullptr;
		Renderer3D* m_Renderer3D = nullptr;
		bool m_NoCameraWarned = false;
	};

}
