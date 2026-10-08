#pragma once

namespace Dingo
{

	class Scene;
	class Renderer2D;
	class Renderer3D;
	class Framebuffer;

	// Renders a Scene by reading its primary CameraComponent (projection) and that
	// camera entity's transform (view), submitting the scene's light components for a 3D
	// pass, then dispatching the 2D or 3D pass to Renderer2D / Renderer3D. Holds non-owning
	// references to the engine renderers (owned by Application). This is the single
	// per-scene render entry point — SceneManager::OnRender() delegates here.
	class SceneRenderer
	{
	public:
		SceneRenderer(Renderer2D& renderer2D, Renderer3D& renderer3D);

		SceneRenderer(const SceneRenderer&) = delete;
		SceneRenderer& operator=(const SceneRenderer&) = delete;

		// Renders the scene's renderable entities through its primary camera, clearing
		// to the scene's clear color. No-op (warns once) if the scene has no camera, and a
		// silent no-op while Renderer::IsFrameSkipped() (a minimized window).
		//
		// Into `target` when one is given (Renderer::SetRenderTarget for the call), else into the
		// current render target; the projections take the aspect of whichever it is. Give the target
		// an RGBA8 colour attachment and depth (FramebufferParams::SetEnableDepth), like the window's
		// framebuffer the renderers build their pipelines against. Sample the result through
		// target->GetAttachment(0): its first row is the top of the picture, the opposite of a
		// texture loaded from a file, so a Renderer2D quad shows it upright with a negative height.
		// shadowProbes: whether this render answers the scene's GetLightVisibility questions
		// (Scene::SubmitLights); pass false for a secondary view, such as a minimap.
		void Render(Scene& scene, Framebuffer* target = nullptr, bool shadowProbes = true);

	private:
		Renderer2D* m_Renderer2D = nullptr;
		Renderer3D* m_Renderer3D = nullptr;
		bool m_NoCameraWarned = false;
	};

}
