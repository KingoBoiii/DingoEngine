#include "depch.h"
#include "DingoEngine/Scene/SceneRenderer.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/Components.h"

#include "DingoEngine/Graphics/PostProcess.h"
#include "DingoEngine/Graphics/Renderer2D.h"
#include "DingoEngine/Graphics/Renderer3D.h"

#include <glm/glm.hpp>

namespace Dingo
{

	SceneRenderer::SceneRenderer(Renderer2D& renderer2D, Renderer3D& renderer3D)
		: m_Renderer2D(&renderer2D), m_Renderer3D(&renderer3D)
	{
	}

	void SceneRenderer::Render(Scene& scene, Framebuffer* target, bool shadowProbes)
	{
		if (Renderer::IsFrameSkipped())
			return;

		DE_PROFILE_SCOPE_TEXT("SceneRenderer::Render", scene.GetName());

		// A scene can carry a perspective (world) camera and/or an orthographic (UI)
		// camera: the 3D world is drawn first, then the 2D entities as an overlay on
		// top. Cameras are found via narrow component views — a full ForEachEntity scan
		// here cost hundreds of lookups per frame in big scenes.
		Entity perspectiveCamera, orthographicCamera;
		bool hasPerspective = false, hasOrthographic = false;
		scene.GetRenderCameras(perspectiveCamera, hasPerspective, orthographicCamera, hasOrthographic);

		if (!hasPerspective && !hasOrthographic)
		{
			if (!m_NoCameraWarned)
			{
				DE_CORE_WARN("SceneRenderer: scene '{}' has no CameraComponent; nothing rendered.", scene.GetName());
				m_NoCameraWarned = true;
			}
			return;
		}

		Framebuffer* previousTarget = Renderer::GetRenderTarget();
		const std::optional<Viewport> previousViewport = Renderer::GetViewport();
		if (target)
			Renderer::SetRenderTarget(target);

		// Aspect comes from the framebuffer drawn into, so projections track the window, or the
		// texture a scene renders into, or the viewport drawn within. Guard a zero height.
		const Framebuffer* drawn = target ? target : previousTarget ? previousTarget : Renderer::GetSwapChainFramebuffer();
		const bool inViewport = !target && previousViewport;
		const float width = inViewport ? previousViewport->Width : static_cast<float>(drawn->GetParams().Width);
		const float height = inViewport ? previousViewport->Height : static_cast<float>(drawn->GetParams().Height);
		const float aspect = height > 0.0f ? width / height : 1.0f;
		const glm::vec4 clearColor = scene.GetClearColor();

		// 3D world pass — clears colour + depth to the scene's clear colour. Lighting comes
		// entirely from this scene's light components (a default light when it has none), so a
		// previous scene's light never bleeds in.
		if (hasPerspective)
		{
			// The post chain takes the 3D pass alone: the 2D overlay below draws into the target as before.
			const PostProcessComponent* post = perspectiveCamera.HasComponent<PostProcessComponent>() ? &perspectiveCamera.GetComponent<PostProcessComponent>() : nullptr;
			const bool postProcess = post && post->Settings.Enabled;
			PostProcessStack& stack = Renderer::GetPostProcessStack();
			if (postProcess)
				stack.Begin(post->Settings, perspectiveCamera.GetComponent<CameraComponent>().GetProjection(aspect));

			m_Renderer3D->BeginScene(scene.GetCameraViewProjection(perspectiveCamera, aspect));
			m_Renderer3D->Clear(clearColor);
			scene.SubmitLights(*m_Renderer3D, shadowProbes);
			scene.RenderEntities3D(*m_Renderer3D);
			m_Renderer3D->EndScene();

			if (postProcess)
				stack.End();
		}

		// 2D pass — clears only when it is the sole pass; as an overlay over the 3D
		// world it must NOT clear, so the world stays visible underneath (UI on top).
		if (hasOrthographic)
		{
			m_Renderer2D->BeginScene(scene.GetCameraViewProjection(orthographicCamera, aspect));
			if (!hasPerspective)
				m_Renderer2D->Clear(clearColor);
			scene.RenderEntities(*m_Renderer2D);
			m_Renderer2D->EndScene();
		}

		if (target)
		{
			Renderer::SetRenderTarget(previousTarget);
			if (previousViewport)
				Renderer::SetViewport(*previousViewport);
		}
	}

}
