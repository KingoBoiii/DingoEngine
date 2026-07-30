#include "depch.h"
#include "DingoEngine/Scene/Systems/CameraUtils.h"

#include "DingoEngine/Scene/Components.h"

#include <glm/gtc/quaternion.hpp>

namespace Dingo
{

	namespace Internal
	{

		namespace CameraUtils
		{

			bool FindPrimaryCamera(const entt::registry& registry, entt::entity& out)
			{
				entt::entity first = entt::null;

				auto view = registry.view<const CameraComponent>();
				for (entt::entity handle : view)
				{
					if (first == entt::null)
						first = handle;

					if (view.get<const CameraComponent>(handle).Primary)
					{
						out = handle;
						return true;
					}
				}

				if (first != entt::null)
				{
					out = first;
					return true;
				}

				return false;
			}

			void FindRenderCameras(const entt::registry& registry, entt::entity& outPerspective, bool& outHasPerspective, entt::entity& outOrthographic, bool& outHasOrthographic)
			{
				outHasPerspective = false;
				outHasOrthographic = false;
				bool perspectivePrimary = false, orthographicPrimary = false;

				auto view = registry.view<const CameraComponent>();
				for (entt::entity handle : view)
				{
					const CameraComponent& camera = view.get<const CameraComponent>(handle);
					if (camera.Type == CameraComponent::ProjectionType::Perspective)
					{
						if (!outHasPerspective || (camera.Primary && !perspectivePrimary))
						{
							outPerspective = handle;
							outHasPerspective = true;
							perspectivePrimary = camera.Primary;
						}
					}
					else if (!outHasOrthographic || (camera.Primary && !orthographicPrimary))
					{
						outOrthographic = handle;
						outHasOrthographic = true;
						orthographicPrimary = camera.Primary;
					}
				}
			}

			bool FindFirstDirectionalLight(const entt::registry& registry, entt::entity& out)
			{
				for (entt::entity handle : registry.view<const DirectionalLightComponent>())
				{
					out = handle;
					return true;
				}

				return false;
			}

			glm::mat4 ViewProjection(const entt::registry& registry, entt::entity camera, float aspect)
			{
				if (!registry.valid(camera) || !registry.all_of<CameraComponent>(camera))
					return glm::mat4(1.0f);

				const CameraComponent& cameraComponent = registry.get<CameraComponent>(camera);
				const glm::mat4 projection = cameraComponent.GetProjection(aspect);

				// The view is the inverse of the camera entity's world transform. A perspective
				// camera reads its Transform3DComponent (position + orientation); an orthographic
				// camera reads the 2D TransformComponent (every entity has one on creation).
				glm::mat4 view(1.0f);
				if (cameraComponent.Type == CameraComponent::ProjectionType::Perspective)
				{
					if (registry.all_of<Transform3DComponent>(camera))
					{
						const Transform3DComponent& transform = registry.get<Transform3DComponent>(camera);
						view = glm::inverse(glm::translate(glm::mat4(1.0f), transform.Position) * glm::mat4_cast(transform.Rotation));
					}
				}
				else
				{
					const TransformComponent& transform = registry.get<TransformComponent>(camera);
					view = glm::inverse(
						glm::translate(glm::mat4(1.0f), glm::vec3(transform.Position.x, transform.Position.y, 0.0f))
						* glm::rotate(glm::mat4(1.0f), glm::radians(transform.Rotation), glm::vec3(0.0f, 0.0f, 1.0f)));
				}

				return projection * view;
			}

			Ray ScreenPointToRay(const entt::registry& registry, const glm::vec2& screenPos, const glm::vec2& viewportSize)
			{
				entt::entity perspective = entt::null, orthographic = entt::null;
				bool hasPerspective = false, hasOrthographic = false;
				FindRenderCameras(registry, perspective, hasPerspective, orthographic, hasOrthographic);

				// Unprojecting through an orthographic camera's parallel projection would not
				// produce a meaningful converging ray, so only a perspective camera qualifies.
				if (!hasPerspective || viewportSize.y <= 0.0f)
					return Ray();

				const float aspect = viewportSize.x / viewportSize.y;
				const CameraComponent& camera = registry.get<CameraComponent>(perspective);
				const glm::mat4 projection = camera.GetProjection(aspect);

				glm::mat4 cameraView(1.0f);
				if (registry.all_of<Transform3DComponent>(perspective))
				{
					const Transform3DComponent& transform = registry.get<Transform3DComponent>(perspective);
					cameraView = glm::inverse(glm::translate(glm::mat4(1.0f), transform.Position) * glm::mat4_cast(transform.Rotation));
				}

				return Dingo::ScreenPointToRay(screenPos, viewportSize, cameraView, projection);
			}

		}

	}

}
