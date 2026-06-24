#include "depch.h"
#include "DingoEngine/Scene/SceneRenderer.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/Components.h"

#include "DingoEngine/Graphics/Renderer2D.h"
#include "DingoEngine/Graphics/Renderer3D.h"

#include <glm/glm.hpp>

namespace Dingo
{

	SceneRenderer::SceneRenderer(Renderer2D& renderer2D, Renderer3D& renderer3D)
		: m_Renderer2D(&renderer2D), m_Renderer3D(&renderer3D)
	{
	}

	void SceneRenderer::Render(Scene& scene)
	{
		// A scene can carry a perspective (world) camera and/or an orthographic (UI)
		// camera: the 3D world is drawn first, then the 2D entities as an overlay on
		// top. For each projection type prefer a camera marked Primary, else the first
		// of that type. The first directional light is collected in the same pass.
		Entity perspectiveCamera, orthographicCamera;
		bool hasPerspective = false, hasOrthographic = false;
		bool perspectivePrimary = false, orthographicPrimary = false;

		DirectionalLightComponent light;
		bool foundLight = false;

		scene.ForEachEntity([&](Entity entity)
		{
			if (entity.HasComponent<CameraComponent>())
			{
				const CameraComponent& camera = entity.GetComponent<CameraComponent>();
				if (camera.Type == CameraComponent::ProjectionType::Perspective)
				{
					if (!hasPerspective || (camera.Primary && !perspectivePrimary))
					{
						perspectiveCamera = entity;
						hasPerspective = true;
						perspectivePrimary = camera.Primary;
					}
				}
				else if (!hasOrthographic || (camera.Primary && !orthographicPrimary))
				{
					orthographicCamera = entity;
					hasOrthographic = true;
					orthographicPrimary = camera.Primary;
				}
			}

			if (!foundLight && entity.HasComponent<DirectionalLightComponent>())
			{
				light = entity.GetComponent<DirectionalLightComponent>();
				foundLight = true;
			}
		});

		if (!hasPerspective && !hasOrthographic)
		{
			if (!m_NoCameraWarned)
			{
				DE_CORE_WARN("SceneRenderer: scene '{}' has no CameraComponent; nothing rendered.", scene.GetName());
				m_NoCameraWarned = true;
			}
			return;
		}

		// Aspect comes from the swap-chain framebuffer, so projections track the window
		// automatically. Guard a zero height (e.g. a minimized window).
		const glm::vec2 viewportSize = m_Renderer2D->GetViewportSize();
		const float aspect = (viewportSize.y > 0.0f) ? viewportSize.x / viewportSize.y : 1.0f;
		const glm::vec4 clearColor = scene.GetClearColor();

		// 3D world pass — clears colour + depth to the scene's clear colour. The light
		// falls back to defaults when the scene has none, so lighting is deterministic
		// per scene (and a previous scene's light doesn't bleed in).
		if (hasPerspective)
		{
			m_Renderer3D->SetDirectionalLight(light.Direction, light.Ambient);

			m_Renderer3D->BeginScene(scene.GetCameraViewProjection(perspectiveCamera, aspect));
			m_Renderer3D->Clear(clearColor);
			scene.RenderEntities3D(*m_Renderer3D);
			m_Renderer3D->EndScene();
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
	}

}
