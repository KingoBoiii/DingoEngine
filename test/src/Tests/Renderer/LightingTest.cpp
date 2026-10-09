#include "LightingTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
	constexpr glm::vec4 k_FloorColor{ 0.72f, 0.72f, 0.70f, 1.0f };
	constexpr glm::vec4 k_PillarColor{ 0.82f, 0.80f, 0.76f, 1.0f };
	constexpr glm::vec4 k_SphereColor{ 0.86f, 0.86f, 0.90f, 1.0f };

	constexpr float k_PillarXs[] = { -6.0f, -2.0f, 2.0f, 6.0f };
	constexpr float k_PillarZs[] = { -3.0f, 3.0f };
	constexpr float k_SphereXs[] = { -4.0f, 0.0f, 4.0f };

	struct ColoredPoint
	{
		glm::vec3 Position;
		glm::vec3 Color;
	};

	constexpr ColoredPoint k_PointLights[] = {
		{ { -4.0f, 1.0f, -3.0f }, { 1.00f, 0.55f, 0.20f } },
		{ {  0.0f, 1.0f, -3.0f }, { 0.25f, 0.55f, 1.00f } },
		{ {  4.0f, 1.0f, -3.0f }, { 0.30f, 1.00f, 0.40f } },
		{ { -4.0f, 1.0f,  3.0f }, { 1.00f, 0.25f, 0.65f } },
		{ {  0.0f, 1.0f,  3.0f }, { 1.00f, 0.85f, 0.45f } },
		{ {  4.0f, 1.0f,  3.0f }, { 0.55f, 0.35f, 1.00f } },
	};

	constexpr int k_OverBudgetColumns = 16;
	constexpr int k_OverBudgetRows = 4;

	// Turned back out of each spot entity's local direction, so the entity path aims exactly like
	// the direct one while still going through LightSystem's Rotation * Direction.
	const glm::quat k_SpotEntityRotation = glm::angleAxis(glm::radians(35.0f), glm::normalize(glm::vec3(1.0f, 0.5f, 0.0f)));

	constexpr glm::vec4 k_RowColor{ 0.70f, 0.12f, 0.10f, 1.0f };
	constexpr glm::vec3 k_LampPosition{ 0.0f, 4.0f, 4.0f };
	constexpr glm::vec3 k_LampColor{ 1.0f, 0.85f, 0.6f };

	glm::mat4 BoxTransform(const glm::vec3& center, const glm::vec3& size)
	{
		return glm::scale(glm::translate(glm::mat4(1.0f), center), size);
	}

	// A fully saturated hue around the colour wheel, t in [0, 1).
	glm::vec3 Hue(float t)
	{
		const glm::vec3 channels = glm::abs(glm::fract(glm::vec3(t) + glm::vec3(1.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f - 3.0f);
		return glm::clamp(channels - 1.0f, 0.0f, 1.0f);
	}
}

namespace Dingo
{

	namespace
	{
		// Fixed, so what is culled doesn't depend on the window.
		glm::mat4 CheckViewProjection()
		{
			return glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f) *
				glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		}

		PointLight PointLightAt(const glm::vec3& position, float range, float intensity = 1.0f)
		{
			PointLight light;
			light.Position = position;
			light.Range = range;
			light.Intensity = intensity;
			return light;
		}

		SpotLight SpotLightAt(const glm::vec3& position, const glm::vec3& direction, float range)
		{
			SpotLight light;
			light.Position = position;
			light.Direction = direction;
			light.Range = range;
			light.InnerConeAngle = 20.0f;
			light.OuterConeAngle = 30.0f;
			return light;
		}

		// A point `distance` from a downward spot at `position`, `degrees` off its axis.
		glm::vec3 OffDownwardAxis(const glm::vec3& position, float degrees, float distance)
		{
			const float angle = glm::radians(degrees);
			return position + distance * glm::vec3(std::sin(angle), -std::cos(angle), 0.0f);
		}

		bool IsNear(float a, float b)
		{
			return std::abs(a - b) <= 1e-5f;
		}

		bool IsNear(const glm::vec3& a, const glm::vec3& b)
		{
			return IsNear(a.x, b.x) && IsNear(a.y, b.y) && IsNear(a.z, b.z);
		}

		template<typename Submit>
		Renderer3D::Statistics RenderCheckScene(Renderer3D& renderer, Submit&& submit)
		{
			renderer.BeginScene(CheckViewProjection());
			submit();
			renderer.EndScene();
			return renderer.GetStatistics();
		}

		template<typename Populate>
		Renderer3D::Statistics RenderCheckEntities(Renderer3D& renderer, Populate&& populate)
		{
			Scene scene("Lighting Check");
			populate(scene);
			return RenderCheckScene(renderer, [&] { scene.SubmitLights(renderer); });
		}

		Entity AddPointLightEntity(Scene& scene, const glm::vec3& position, float range)
		{
			Entity entity = scene.CreateEntity("Point Light");
			entity.AddComponent<Transform3DComponent>().Position = position;
			entity.AddComponent<PointLightComponent>(PointLightComponent(glm::vec3(1.0f), 1.0f, range));
			return entity;
		}

		constexpr uint32_t k_FogProbeSize = 16;
		constexpr float k_FogProbeTolerance = 2.5f / 255.0f;

		// A black wall 10 units in front of CheckViewProjection's camera, filling its view, under a white
		// ambient: unfogged it reads 0, so under a white fog the centre pixel reads the fog factor.
		template<typename Submit>
		void RenderFogProbe(Renderer3D& renderer, Framebuffer* target, Submit&& submit)
		{
			Framebuffer* previous = Renderer::GetRenderTarget();
			Renderer::SetRenderTarget(target);
			renderer.BeginScene(CheckViewProjection());
			renderer.Clear({ 0.0f, 0.0f, 0.0f, 1.0f });
			submit();
			renderer.SubmitMesh(renderer.GetBoxMesh(), BoxTransform({ 0.0f, 0.0f, -10.5f }, { 60.0f, 40.0f, 1.0f }), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			renderer.EndScene();
			Renderer::SetRenderTarget(previous);
		}

		Fog WhiteFog(FogMode mode, float start, float end, float density = 0.0f)
		{
			Fog fog;
			fog.Mode = mode;
			fog.Color = glm::vec3(1.0f);
			fog.Start = start;
			fog.End = end;
			fog.Density = density;
			return fog;
		}

		std::string Counts(const Renderer3D::Statistics& stats)
		{
			return std::format("directional {}, local {}, culled {}, dropped {}",
				stats.DirectionalLights, stats.LocalLights, stats.CulledLights, stats.DroppedLights);
		}
	}

	void LightingTest::BuildCheckSteps()
	{
		m_CheckSteps.clear();

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckScene(*m_CheckRenderer, [] {});
			Check(stats.DirectionalLights == 1 && stats.LocalLights == 0,
				std::format("no light submitted: the default light is used ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckScene(*m_CheckRenderer, [this] { m_CheckRenderer->SetAmbientLight(glm::vec3(0.0f), 0.0f); });
			Check(stats.DirectionalLights == 0,
				std::format("ambient alone: the default light is switched off ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckScene(*m_CheckRenderer, [this]
			{
				m_CheckRenderer->SubmitLight(PointLightAt({ 0.0f, 0.0f, -10.0f }, 5.0f, 0.0f));
			});
			Check(stats.LocalLights == 0 && stats.CulledLights == 0 && stats.DirectionalLights == 0,
				std::format("an unusable point light is ignored, yet still switches the default light off ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckScene(*m_CheckRenderer, [this]
			{
				const DirectionalLight light{};
				for (int i = 0; i < 5; i++)
					m_CheckRenderer->SubmitLight(light);
			});
			Check(stats.DirectionalLights == 4 && stats.DroppedLights == 1,
				std::format("five directional lights: four are used, one is dropped ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			Renderer3DParams params;
			params.Capabilities.MaxLocalLights = 100;
			Renderer3D* renderer = Renderer3D::Create(params);
			const uint32_t budget = renderer->GetLocalLightBudget();
			renderer->Shutdown();
			delete renderer;
			Check(budget == 32, std::format("MaxLocalLights above the limit: the light budget is capped at 32 (budget {})", budget));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckScene(*m_BudgetCheckRenderer, [this]
			{
				for (int x = -3; x <= 2; x++)
					m_BudgetCheckRenderer->SubmitLight(PointLightAt({ static_cast<float>(x), 0.0f, -10.0f }, 1.0f));
				for (int x : { -1, 1 })
					m_BudgetCheckRenderer->SubmitLight(PointLightAt({ static_cast<float>(x), 0.0f, 10.0f }, 1.0f));
			});
			Check(stats.LocalLights == 4 && stats.CulledLights == 2 && stats.DroppedLights == 2,
				std::format("6 point lights in view and 2 behind the camera, budget 4: 4 used, 2 culled, 2 dropped ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			m_CheckRenderer->SubmitLight(PointLightAt({ 0.0f, 0.0f, -10.0f }, 5.0f));
			const auto stats = RenderCheckScene(*m_CheckRenderer, [] {});
			Check(stats.LocalLights == 1,
				std::format("a light submitted before BeginScene lights that scene ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckScene(*m_CheckRenderer, [] {});
			Check(stats.DirectionalLights == 1 && stats.LocalLights == 0,
				std::format("lights don't carry over: the next empty scene is back on the default light ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckEntities(*m_CheckRenderer, [](Scene& scene)
			{
				scene.CreateEntity("Empty");
			});
			Check(stats.DirectionalLights == 1,
				std::format("scene without light components: the default light is used ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckEntities(*m_CheckRenderer, [](Scene& scene)
			{
				Entity light = AddPointLightEntity(scene, { 0.0f, 0.0f, -10.0f }, 5.0f);
				light.GetComponent<PointLightComponent>().Enabled = false;
			});
			Check(stats.DirectionalLights == 0 && stats.LocalLights == 0,
				std::format("disabled point light: not drawn, yet still switches the default light off ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckEntities(*m_CheckRenderer, [](Scene& scene)
			{
				Entity light = scene.CreateEntity("Unplaced Light");
				light.AddComponent<PointLightComponent>();
			});
			Check(stats.DirectionalLights == 1 && stats.LocalLights == 0,
				std::format("point light without a Transform3DComponent: ignored, and the default light stays on ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckEntities(*m_CheckRenderer, [](Scene& scene)
			{
				for (int i = 0; i < 2; i++)
					scene.CreateEntity("Sun").AddComponent<DirectionalLightComponent>();
			});
			Check(stats.DirectionalLights == 2,
				std::format("two directional light components: both are used ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckEntities(*m_CheckRenderer, [](Scene& scene)
			{
				Entity light = AddPointLightEntity(scene, { 0.0f, 0.0f, -10.0f }, 5.0f);
				scene.DuplicateEntity(light);
			});
			Check(stats.LocalLights == 2,
				std::format("a duplicated point light entity is a second light ({})", Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			const PointLight point = PointLightAt({ 1.0f, 2.0f, 3.0f }, 4.0f);
			const float centre = GetLightAttenuation(point, point.Position);
			const float halfRange = GetLightAttenuation(point, point.Position + glm::vec3(2.0f, 0.0f, 0.0f));
			const float atRange = GetLightAttenuation(point, point.Position + glm::vec3(0.0f, 0.0f, 4.0f));
			const float beyond = GetLightAttenuation(point, point.Position + glm::vec3(0.0f, -4.5f, 0.0f));
			Check(IsNear(centre, 1.0f), std::format("point light attenuation is 1 at its centre ({:.6f})", centre));
			Check(IsNear(halfRange, 0.5625f), std::format("point light attenuation is 0.5625 at half its range ({:.6f})", halfRange));
			Check(atRange == 0.0f && beyond == 0.0f,
				std::format("point light attenuation is exactly 0 at its range and beyond ({}, {})", atRange, beyond));

			const SpotLight spot = SpotLightAt({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 4.0f);
			const float onAxis = GetLightAttenuation(spot, OffDownwardAxis(spot.Position, 0.0f, 2.0f));
			const float insideInner = GetLightAttenuation(spot, OffDownwardAxis(spot.Position, 10.0f, 2.0f));
			Check(IsNear(onAxis, 0.5625f) && IsNear(insideInner, 0.5625f),
				std::format("spot light attenuation is 0.5625 at half its range on its axis and 10 degrees off it, inside the inner cone ({:.6f}, {:.6f})", onAxis, insideInner));

			const double cosInner = std::cos(glm::radians(20.0));
			const double cosOuter = std::cos(glm::radians(30.0));
			const double cone = (std::cos(glm::radians(25.0)) - cosOuter) / (cosInner - cosOuter);
			const float expected = static_cast<float>(0.75 * 0.75 * cone * cone);
			const float between = GetLightAttenuation(spot, OffDownwardAxis(spot.Position, 25.0f, 2.0f));
			Check(IsNear(between, expected),
				std::format("spot light attenuation 25 degrees off its axis, between the 20 and 30 degree cones, is falloff^2 * cone^2 ({:.6f}, expected {:.6f})", between, expected));

			const float atOuter = GetLightAttenuation(spot, OffDownwardAxis(spot.Position, 30.0f, 2.0f));
			const float pastOuter = GetLightAttenuation(spot, OffDownwardAxis(spot.Position, 31.0f, 2.0f));
			const float side = GetLightAttenuation(spot, OffDownwardAxis(spot.Position, 90.0f, 2.0f));
			const float behind = GetLightAttenuation(spot, OffDownwardAxis(spot.Position, 180.0f, 2.0f));
			Check(IsNear(atOuter, 0.0f) && pastOuter == 0.0f && side == 0.0f && behind == 0.0f,
				std::format("spot light attenuation is 0 at its outer angle and past it ({}, {}, {}, {})", atOuter, pastOuter, side, behind));

			const float nan = std::numeric_limits<float>::quiet_NaN();
			const glm::vec3 spotSample = OffDownwardAxis(spot.Position, 0.0f, 2.0f);
			std::vector<std::pair<const char*, float>> rejected;
			const auto rejectBoth = [&](const char* what, auto&& spoil)
			{
				PointLight badPoint = point;
				SpotLight badSpot = spot;
				spoil(badPoint, badSpot);
				rejected.emplace_back(what, (std::max)(GetLightAttenuation(badPoint, point.Position), GetLightAttenuation(badSpot, spotSample)));
			};
			rejectBoth("Intensity 0", [](PointLight& p, SpotLight& s) { p.Intensity = 0.0f; s.Intensity = 0.0f; });
			rejectBoth("Range 0", [](PointLight& p, SpotLight& s) { p.Range = 0.0f; s.Range = 0.0f; });
			rejectBoth("Range -1", [](PointLight& p, SpotLight& s) { p.Range = -1.0f; s.Range = -1.0f; });
			rejectBoth("NaN position", [nan](PointLight& p, SpotLight& s) { p.Position.x = nan; s.Position.x = nan; });
			rejectBoth("Color x Intensity overflows", [](PointLight& p, SpotLight& s) { p.Color = glm::vec3(1e30f); p.Intensity = 1e10f; s.Color = glm::vec3(1e30f); s.Intensity = 1e10f; });
			SpotLight nanCone = spot;
			nanCone.OuterConeAngle = nan;
			rejected.emplace_back("NaN cone angle", GetLightAttenuation(nanCone, spotSample));

			bool allZero = true;
			std::string values;
			for (const auto& [what, value] : rejected)
			{
				allZero = allZero && value == 0.0f;
				values += std::format("{}{} {}", values.empty() ? "" : ", ", what, value);
			}
			Check(allZero, std::format("a light SubmitLight rejects has attenuation 0 ({})", values));

			const SpotLight aimless = SpotLightAt(spot.Position, glm::vec3(0.0f), spot.Range);
			bool sameAsDown = true;
			for (float degrees : { 0.0f, 25.0f, 90.0f, 180.0f })
			{
				const glm::vec3 sample = OffDownwardAxis(spot.Position, degrees, 2.0f);
				sameAsDown = sameAsDown && GetLightAttenuation(aimless, sample) == GetLightAttenuation(spot, sample);
			}
			const float aimlessOnAxis = GetLightAttenuation(aimless, OffDownwardAxis(spot.Position, 0.0f, 2.0f));
			Check(sameAsDown && IsNear(aimlessOnAxis, 0.5625f),
				std::format("a spot light with a zero-length direction points down (0, -1, 0) (on the axis: {:.6f})", aimlessOnAxis));

			Transform3DComponent transform(glm::vec3(1.0f, 2.0f, 3.0f));
			transform.Rotation = glm::angleAxis(glm::radians(40.0f), glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f)));
			SpotLightComponent component({ 1.0f, 0.5f, 0.25f }, 2.0f, 7.0f);
			component.Direction = { 0.0f, -0.6f, -0.8f };
			component.InnerConeAngle = 15.0f;
			component.OuterConeAngle = 25.0f;
			const SpotLight built = component.ToLight(transform);
			const glm::vec3 worldDirection = transform.Rotation * component.Direction;
			const float alongAxis = GetLightAttenuation(built, transform.Position + worldDirection * 3.5f);
			Check(IsNear(built.Direction, worldDirection) && built.Position == transform.Position && built.Color == component.Color &&
				built.Intensity == component.Intensity && built.Range == component.Range &&
				built.InnerConeAngle == component.InnerConeAngle && built.OuterConeAngle == component.OuterConeAngle && IsNear(alongAxis, 0.5625f),
				std::format("SpotLightComponent::ToLight on a rotated transform: its position, Rotation * Direction ({:.4f}, {:.4f}, {:.4f}), the rest copied (on the axis at half range: {:.6f})",
					built.Direction.x, built.Direction.y, built.Direction.z, alongAxis));

			const PointLightComponent pointComponent({ 0.2f, 0.4f, 0.6f }, 1.5f, 6.0f);
			const PointLight builtPoint = pointComponent.ToLight(transform);
			Check(builtPoint.Position == transform.Position && builtPoint.Color == pointComponent.Color &&
				builtPoint.Intensity == pointComponent.Intensity && builtPoint.Range == pointComponent.Range,
				"PointLightComponent::ToLight: the transform's position, the rest copied");
		});

		m_CheckSteps.push_back([this]
		{
			const SpotLight spot = SpotLightAt({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 4.0f);
			const auto sameWeights = [&](const SpotLight& a, const SpotLight& b, std::initializer_list<float> degrees)
			{
				for (const float angle : degrees)
				{
					const glm::vec3 sample = OffDownwardAxis(spot.Position, angle, 2.0f);
					if (GetLightAttenuation(a, sample) != GetLightAttenuation(b, sample))
						return false;
				}
				return true;
			};

			SpotLight outer179 = spot;
			outer179.OuterConeAngle = 179.0f;
			SpotLight outer200 = spot;
			outer200.OuterConeAngle = 200.0f;
			SpotLight outer1 = spot;
			outer1.OuterConeAngle = 1.0f;
			SpotLight outerTiny = spot;
			outerTiny.OuterConeAngle = 0.2f;
			SpotLight innerPastOuter = spot;
			innerPastOuter.InnerConeAngle = 40.0f;
			SpotLight innerEqualsOuter = spot;
			innerEqualsOuter.InnerConeAngle = spot.OuterConeAngle;

			const float wideSide = GetLightAttenuation(outer179, OffDownwardAxis(spot.Position, 90.0f, 2.0f));
			const float hardInside = GetLightAttenuation(innerPastOuter, OffDownwardAxis(spot.Position, 29.0f, 2.0f));
			const float hardOutside = GetLightAttenuation(innerPastOuter, OffDownwardAxis(spot.Position, 31.0f, 2.0f));
			Check(sameWeights(outer200, outer179, { 0.0f, 25.0f, 90.0f, 170.0f, 178.5f }) && wideSide > 0.0f &&
				sameWeights(outerTiny, outer1, { 0.0f, 0.5f, 2.0f }) &&
				sameWeights(innerPastOuter, innerEqualsOuter, { 0.0f, 10.0f, 29.0f, 31.0f, 90.0f }) && hardInside > 0.0f && hardOutside == 0.0f,
				std::format("the cone angles are clamped as SubmitLight clamps them: an outer angle of 200 acts as 179 (the side weighs {:.6f}), 0.2 as 1, and an inner angle past the outer acts as equal to it, a hard edge at 30 degrees ({:.6f} inside, {} outside)",
					wideSide, hardInside, hardOutside));
		});

		m_CheckSteps.push_back([this]
		{
			const float infinity = std::numeric_limits<float>::infinity();
			const PointLight point = PointLightAt({ 1.0f, 2.0f, 3.0f }, infinity);
			const SpotLight spot = SpotLightAt({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, infinity);
			const float pointNear = GetLightAttenuation(point, point.Position + glm::vec3(1.0f, 0.0f, 0.0f));
			const float pointFar = GetLightAttenuation(point, point.Position + glm::vec3(0.0f, 0.0f, 1.0e6f));
			const float spotNear = GetLightAttenuation(spot, OffDownwardAxis(spot.Position, 0.0f, 2.0f));
			const float spotFar = GetLightAttenuation(spot, OffDownwardAxis(spot.Position, 0.0f, 1.0e6f));
			Check(IsNear(pointNear, 1.0f) && IsNear(pointFar, 1.0f) && IsNear(spotNear, 1.0f) && IsNear(spotFar, 1.0f),
				std::format("a light with an infinite Range has falloff 1 at any finite distance (point {:.6f} at 1 m and {:.6f} at 1000 km, spot on its axis {:.6f} and {:.6f})",
					pointNear, pointFar, spotNear, spotFar));
		});

		BuildFogCheckSteps();
	}

	void LightingTest::BuildFogCheckSteps()
	{
		m_CheckSteps.push_back([this]
		{
			bool accepted = false;
			const auto stats = RenderCheckScene(*m_CheckRenderer, [this, &accepted] { accepted = m_CheckRenderer->SetFog(WhiteFog(FogMode::Linear, 5.0f, 15.0f)); });
			Check(accepted && stats.Fogged && stats.DirectionalLights == 1,
				std::format("SetFog fogs the scene and, being no light, keeps the default light (accepted {}, fogged {}, {})", accepted, stats.Fogged, Counts(stats)));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckScene(*m_CheckRenderer, [] {});
			Check(!stats.Fogged, "fog is scene-scoped: the scene after a fogged one has none");
		});

		m_CheckSteps.push_back([this]
		{
			bool backwards = true, notFinite = true, negative = true, none = false;
			const auto stats = RenderCheckScene(*m_CheckRenderer, [&, this]
			{
				backwards = m_CheckRenderer->SetFog(WhiteFog(FogMode::Linear, 20.0f, 10.0f));
				notFinite = m_CheckRenderer->SetFog(WhiteFog(FogMode::Exponential, 0.0f, 0.0f, std::numeric_limits<float>::quiet_NaN()));
				negative = m_CheckRenderer->SetFog(WhiteFog(FogMode::Exponential, 0.0f, 0.0f, -0.1f));
				m_CheckRenderer->SetFog(WhiteFog(FogMode::Exponential, 0.0f, 0.0f, 0.1f));
				Fog off;
				off.Mode = FogMode::None;
				none = m_CheckRenderer->SetFog(off);
			});
			Check(!backwards && !notFinite && !negative && none && !stats.Fogged,
				std::format("SetFog ignores a Linear fog ending before its Start, a NaN and a negative Density, and FogMode::None clears a fog (accepted {} {} {}, None {}, fogged {})",
					backwards, notFinite, negative, none, stats.Fogged));
		});

		m_CheckSteps.push_back([this]
		{
			m_CheckRenderer->BeginScene(glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -50.0f, 50.0f));
			m_CheckRenderer->SetFog(WhiteFog(FogMode::Linear, 0.0f, 1.0f));
			m_CheckRenderer->EndScene();
			Check(!m_CheckRenderer->GetStatistics().Fogged, "an orthographic camera's scene draws without fog");
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckEntities(*m_CheckRenderer, [](Scene& scene)
			{
				scene.CreateEntity("Disabled Fog").AddComponent<FogComponent>().Enabled = false;
				scene.CreateEntity("Fog").AddComponent<FogComponent>();
			});
			Check(stats.Fogged && stats.DirectionalLights == 1,
				std::format("an enabled FogComponent fogs the scene, a disabled one before it doesn't stop it, and neither counts as a light ({}, fogged {})", Counts(stats), stats.Fogged));
		});

		m_CheckSteps.push_back([this]
		{
			const auto stats = RenderCheckEntities(*m_CheckRenderer, [](Scene& scene)
			{
				scene.CreateEntity("Disabled Fog").AddComponent<FogComponent>().Enabled = false;
			});
			Check(!stats.Fogged, "a scene whose only FogComponent is disabled has no fog");
		});

		auto probeStep = [this](const char* name, const glm::vec3& expected, std::function<void(Renderer3D&)> submit)
		{
			m_CheckSteps.push_back([this, name, expected, submit = std::move(submit)]
			{
				Framebuffer* target = Framebuffer::Create(FramebufferParams()
					.SetDebugName("LightingTest_FogProbe")
					.SetWidth(static_cast<int32_t>(k_FogProbeSize))
					.SetHeight(static_cast<int32_t>(k_FogProbeSize))
					.SetEnableDepth(true)
					.AddAttachment({ TextureFormat::RGBA8_UNORM }));
				m_FogTargets.push_back(target);
				RenderFogProbe(*m_CheckRenderer, target, [this, &submit]
				{
					m_CheckRenderer->SetAmbientLight(glm::vec3(1.0f), 1.0f);
					submit(*m_CheckRenderer);
				});
				ReadFogProbe(target, expected, name);
			});
		};

		probeStep("without fog the probe wall reads black", glm::vec3(0.0f), [](Renderer3D&) {});
		probeStep("a linear fog from 5 to 15 is half way at 10 units", glm::vec3(0.5f),
			[](Renderer3D& renderer) { renderer.SetFog(WhiteFog(FogMode::Linear, 5.0f, 15.0f)); });
		probeStep("an exponential fog of density 0.1 is 1 - e^-1 at 10 units", glm::vec3(1.0f - std::exp(-1.0f)),
			[](Renderer3D& renderer) { renderer.SetFog(WhiteFog(FogMode::Exponential, 0.0f, 0.0f, 0.1f)); });
		probeStep("a squared exponential fog of density 0.05 is 1 - e^-0.25 at 10 units", glm::vec3(1.0f - std::exp(-0.25f)),
			[](Renderer3D& renderer) { renderer.SetFog(WhiteFog(FogMode::ExponentialSquared, 0.0f, 0.0f, 0.05f)); });
		probeStep("MaxOpacity 0.5 caps a full fog at half", glm::vec3(0.5f), [](Renderer3D& renderer)
		{
			Fog fog = WhiteFog(FogMode::Linear, 1.0f, 2.0f);
			fog.MaxOpacity = 0.5f;
			renderer.SetFog(fog);
		});

		const glm::vec3 clearColor{ 0.25f, 0.5f, 0.75f };
		probeStep("a FogComponent with UseClearColor fogs into the scene's clear colour", clearColor, [clearColor](Renderer3D& renderer)
		{
			Scene scene("Lighting Fog Check");
			scene.SetClearColor(glm::vec4(clearColor, 1.0f));
			FogComponent& fog = scene.CreateEntity("Fog").AddComponent<FogComponent>(FogComponent(FogMode::Linear, 1.0f, 2.0f));
			fog.Color = glm::vec3(1.0f, 0.0f, 0.0f);
			scene.SubmitLights(renderer);
		});
	}

	void LightingTest::ReadFogProbe(Framebuffer* target, const glm::vec3& expected, const std::string& name)
	{
		const std::weak_ptr<int> alive = m_Alive;
		target->GetAttachment(0)->ReadPixels([this, alive, expected, name](const TexturePixels& pixels)
		{
			if (alive.expired())
				return;

			const glm::vec3 pixel(pixels.GetPixel(pixels.Width / 2, pixels.Height / 2));
			const bool matches = !pixels.Data.empty() && glm::all(glm::lessThanEqual(glm::abs(pixel - expected), glm::vec3(k_FogProbeTolerance)));
			Check(matches, std::format("{} (expected {:.3f} {:.3f} {:.3f}, read {:.3f} {:.3f} {:.3f})",
				name, expected.r, expected.g, expected.b, pixel.r, pixel.g, pixel.b));
		});
	}

	void LightingTest::RunNextCheckStep()
	{
		// A skipped frame (minimized window) draws nothing and leaves the statistics as they were.
		if (Renderer::IsFrameSkipped())
			return;

		if (m_NextCheckStep < m_CheckSteps.size())
			m_CheckSteps[m_NextCheckStep++]();
	}

	void LightingTest::Initialize()
	{
		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 100.0f);
		m_Camera.SetPosition({ 0.0f, 9.0f, 13.0f });
		m_Camera.SetTarget({ 0.0f, 0.0f, 0.0f });

		const ApplicationCommandLineArgs& args = Application::Get().GetCommandLineArgs();
		if (auto mode = args.Get("lighting"))
		{
			if (*mode == "default")
				m_Mode = Mode::DefaultLight;
			else if (*mode == "lights")
				m_Mode = Mode::PointAndSpot;
			else if (*mode == "overbudget")
				m_Mode = Mode::OverBudget;
			else if (*mode == "materials")
				m_Mode = Mode::Materials;
			else
				DE_WARN("Lighting Test: unknown --lighting={}; use default, lights, overbudget or materials.", *mode);
		}
		m_UseEntities = args.Get("entities").has_value();
		m_PostProcess = args.Get("post").has_value();
		m_Bloom = args.Get("bloom").has_value();
		m_AmbientOcclusion = args.Get("ao").has_value();
		m_Fog = args.Get("fog").has_value();
		if (auto fade = args.Get("budget-fade"))
		{
			const float band = fade->empty() ? 0.5f : std::strtof(std::string(*fade).c_str(), nullptr);
			m_BudgetFade = band > 0.0f ? band : 0.5f;
		}
		if (auto specular = args.Get("specular"))
			m_Specular = *specular != "off";

		BuildScene();

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		for (int i = 0; i < k_RoughnessSteps; i++)
		{
			const float roughness = static_cast<float>(i) / static_cast<float>(k_RoughnessSteps - 1);
			m_RowMaterials[i] = renderer.CreateLitMaterial(MaterialParams()
				.SetDebugName("LightingTest_Roughness")
				.SetRoughness(roughness));
		}

		m_LampMaterial = renderer.CreateLitMaterial(MaterialParams()
			.SetDebugName("LightingTest_Lamp")
			.SetEmissiveColor(k_LampColor)
			.SetEmissiveStrength(1.5f));

		m_CrateMaterial = renderer.CreateLitMaterial(MaterialParams()
			.SetDebugName("LightingTest_Crate")
			.SetRoughness(0.4f));
		m_CrateTexture = Application::Get().GetAssetManager().LoadAsync("textures/container.jpg");

		m_Checks.clear();
		m_NextCheckStep = 0;
		m_Alive = std::make_shared<int>(0);
		Renderer3DParams budgetParams;
		budgetParams.Capabilities.MaxLocalLights = 4;
		m_CheckRenderer = Renderer3D::Create();
		m_BudgetCheckRenderer = Renderer3D::Create(budgetParams);
		BuildCheckSteps();
	}

	void LightingTest::Update(float deltaTime)
	{
		if (m_Animate)
			m_Time += deltaTime;

		const Lighting lighting = DescribeLighting();

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		PostProcessSettings post;
		renderer.SetLightBudgetFade(m_BudgetFade);
		post.Enabled = m_PostProcess || m_Bloom || m_AmbientOcclusion;
		post.Bloom.Enabled = m_Bloom;
		post.AmbientOcclusion.Enabled = m_AmbientOcclusion;
		Renderer::GetPostProcessStack().Begin(post, m_Camera.GetProjectionMatrix());

		// The entity path begins the scene the way SceneRenderer does, from the view-projection
		// alone, so the camera position the renderer rebuilds from it is covered too.
		if (m_UseEntities && m_Mode != Mode::Materials)
			renderer.BeginScene(m_Camera.GetViewProjectionMatrix());
		else
			renderer.BeginScene(m_Camera);
		renderer.Clear(m_ClearColor);

		if (m_Mode == Mode::Materials)
		{
			SubmitLights(renderer, lighting);
			DrawMaterialsScene(renderer);
		}
		else if (m_UseEntities)
		{
			if (!m_LightEntitiesBuilt || m_LightEntitiesMode != m_Mode)
				BuildLightEntities(lighting);
			UpdateLightEntities(lighting);

			const Fog fog = DescribeFog();
			FogComponent& fogComponent = m_FogEntity.GetComponent<FogComponent>();
			fogComponent.Enabled = m_Fog;
			fogComponent.Start = fog.Start;
			fogComponent.End = fog.End;
			m_Scene->SetClearColor(m_ClearColor);
			m_Scene->SubmitLights(renderer);
			m_Scene->RenderEntities3D(renderer);
		}
		else
		{
			SubmitLights(renderer, lighting);
			DrawScene(renderer);
		}

		if (m_Fog && !(m_UseEntities && m_Mode != Mode::Materials))
			renderer.SetFog(DescribeFog());

		renderer.EndScene();
		Renderer::GetPostProcessStack().End();

		RunNextCheckStep();
	}

	LightingTest::Lighting LightingTest::DescribeLighting() const
	{
		Lighting lighting;

		switch (m_Mode)
		{
			case Mode::DefaultLight:
				break;

			case Mode::PointAndSpot:
			{
				lighting.UsesDefaultLight = false;
				lighting.AmbientColor = { 0.5f, 0.6f, 0.9f };
				lighting.AmbientIntensity = 0.06f;

				for (int i = 0; i < static_cast<int>(std::size(k_PointLights)); i++)
				{
					const float phase = m_Time * 1.5f + static_cast<float>(i);
					PointLight light;
					light.Position = k_PointLights[i].Position + 0.8f * glm::vec3(std::cos(phase), 0.0f, std::sin(phase)) * (m_Animate ? 1.0f : 0.0f);
					light.Color = k_PointLights[i].Color;
					light.Intensity = 1.6f;
					light.Range = 4.5f;
					lighting.PointLights.push_back(light);
				}

				const float sweep = m_Animate ? 0.35f * std::sin(m_Time) : 0.0f;
				for (float x : { -4.0f, 4.0f })
				{
					SpotLight spot;
					spot.Position = { x, 7.0f, 0.0f };
					spot.Direction = { sweep, -1.0f, 0.0f };
					spot.Color = x < 0.0f ? glm::vec3(1.0f, 0.9f, 0.75f) : glm::vec3(0.75f, 0.85f, 1.0f);
					spot.Intensity = 1.8f;
					spot.Range = 12.0f;
					spot.InnerConeAngle = 12.0f;
					spot.OuterConeAngle = 20.0f;
					lighting.SpotLights.push_back(spot);
				}
				break;
			}

			case Mode::OverBudget:
			{
				lighting.UsesDefaultLight = false;
				lighting.AmbientColor = { 0.5f, 0.6f, 0.9f };
				lighting.AmbientIntensity = 0.04f;

				for (int row = 0; row < k_OverBudgetRows; row++)
				{
					for (int column = 0; column < k_OverBudgetColumns; column++)
					{
						const int index = row * k_OverBudgetColumns + column;
						PointLight light;
						light.Position = { -9.0f + 1.2f * static_cast<float>(column), 0.4f, -3.75f + 2.5f * static_cast<float>(row) };
						light.Color = Hue(static_cast<float>(index) / static_cast<float>(k_OverBudgetColumns * k_OverBudgetRows));
						light.Intensity = 1.5f;
						light.Range = 1.6f;
						lighting.PointLights.push_back(light);
					}
				}

				// Bright but out of view, behind the camera and far to the sides: they must not
				// take budget slots from the lights on screen.
				for (const glm::vec3& position : { glm::vec3(0.0f, 9.0f, 18.0f), glm::vec3(-40.0f, 0.4f, 0.0f), glm::vec3(40.0f, 0.4f, 0.0f) })
				{
					PointLight light;
					light.Position = position;
					light.Intensity = 10.0f;
					light.Range = 1.6f;
					lighting.PointLights.push_back(light);
				}
				break;
			}

			case Mode::Materials:
			{
				lighting.UsesDefaultLight = false;
				lighting.AmbientColor = { 0.5f, 0.6f, 0.9f };
				lighting.AmbientIntensity = 0.06f;
				lighting.DirectionalLights.push_back(DirectionalLight{ { 0.3f, -1.0f, -0.5f }, { 0.6f, 0.7f, 1.0f }, 0.25f });

				PointLight lamp;
				lamp.Position = k_LampPosition;
				lamp.Color = k_LampColor;
				lamp.Intensity = 1.6f;
				lamp.Range = 16.0f;
				lighting.PointLights.push_back(lamp);
				break;
			}
		}

		return lighting;
	}

	Fog LightingTest::DescribeFog() const
	{
		Fog fog;
		fog.Mode = FogMode::Linear;
		fog.Color = glm::vec3(m_ClearColor);
		fog.Start = 10.0f;
		fog.End = 28.0f;
		return fog;
	}

	void LightingTest::SubmitLights(Renderer3D& renderer, const Lighting& lighting) const
	{
		if (lighting.UsesDefaultLight)
			return;

		renderer.SetAmbientLight(lighting.AmbientColor, lighting.AmbientIntensity);
		for (const DirectionalLight& light : lighting.DirectionalLights)
			renderer.SubmitLight(light);
		for (const PointLight& light : lighting.PointLights)
			renderer.SubmitLight(light);
		for (const SpotLight& light : lighting.SpotLights)
			renderer.SubmitLight(light);
	}

	void LightingTest::DrawScene(Renderer3D& renderer) const
	{
		renderer.DrawBox(BoxTransform({ 0.0f, -0.1f, 0.0f }, { 20.0f, 0.2f, 12.0f }), k_FloorColor);

		for (float x : k_PillarXs)
		{
			for (float z : k_PillarZs)
				renderer.DrawBox(BoxTransform({ x, 1.2f, z }, { 0.8f, 2.4f, 0.8f }), k_PillarColor);
		}

		for (float x : k_SphereXs)
			renderer.DrawSphere(BoxTransform({ x, 0.6f, 0.0f }, glm::vec3(1.2f)), k_SphereColor);
	}

	void LightingTest::DrawMaterialsScene(Renderer3D& renderer)
	{
		renderer.DrawBox(BoxTransform({ 0.0f, -0.1f, 0.0f }, { 20.0f, 0.2f, 12.0f }), k_FloorColor);

		for (int i = 0; i < k_RoughnessSteps; i++)
		{
			m_RowMaterials[i]->SetSpecular(m_Specular ? 0.6f : 0.0f);
			const float x = -6.0f + 3.0f * static_cast<float>(i);
			renderer.SubmitMesh(renderer.GetSphereMesh(), BoxTransform({ x, 0.9f, 0.0f }, glm::vec3(1.8f)), k_RowColor, m_RowMaterials[i]);
		}

		renderer.SubmitMesh(renderer.GetSphereMesh(), BoxTransform(k_LampPosition, glm::vec3(0.5f)), glm::vec4(1.0f), m_LampMaterial);

		if (Texture* crateTexture = Application::Get().GetAssetManager().GetTexture(m_CrateTexture))
			m_CrateMaterial->SetTexture(0, crateTexture);
		m_CrateMaterial->SetSpecular(m_Specular ? 0.35f : 0.0f);
		renderer.SubmitMesh(renderer.GetBoxMesh(), BoxTransform({ 0.0f, 1.0f, -3.5f }, glm::vec3(2.0f)), glm::vec4(1.0f), m_CrateMaterial);
	}

	void LightingTest::BuildScene()
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		m_Scene = new Scene("Lighting Test");

		auto addMesh = [this](const char* name, Mesh* mesh, const glm::vec3& position, const glm::vec3& scale, const glm::vec4& color)
		{
			Entity entity = m_Scene->CreateEntity(name);
			auto& transform = entity.AddComponent<Transform3DComponent>();
			transform.Position = position;
			transform.Scale = scale;
			entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, color));
		};

		addMesh("Floor", renderer.GetBoxMesh(), { 0.0f, -0.1f, 0.0f }, { 20.0f, 0.2f, 12.0f }, k_FloorColor);
		for (float x : k_PillarXs)
		{
			for (float z : k_PillarZs)
				addMesh("Pillar", renderer.GetBoxMesh(), { x, 1.2f, z }, { 0.8f, 2.4f, 0.8f }, k_PillarColor);
		}
		for (float x : k_SphereXs)
			addMesh("Sphere", renderer.GetSphereMesh(), { x, 0.6f, 0.0f }, glm::vec3(1.2f), k_SphereColor);

		m_FogEntity = m_Scene->CreateEntity("Fog");
		m_FogEntity.AddComponent<FogComponent>().Enabled = false;
	}

	void LightingTest::BuildLightEntities(const Lighting& lighting)
	{
		for (Entity entity : m_LightEntities)
			m_Scene->DestroyEntity(entity);
		m_LightEntities.clear();

		m_LightEntitiesMode = m_Mode;
		m_LightEntitiesBuilt = true;

		// No light entity at all leaves the scene on its default light.
		if (lighting.UsesDefaultLight)
			return;

		Entity ambient = m_Scene->CreateEntity("Ambient");
		ambient.AddComponent<AmbientLightComponent>(AmbientLightComponent(lighting.AmbientColor, lighting.AmbientIntensity));
		m_LightEntities.push_back(ambient);

		for (size_t i = 0; i < lighting.PointLights.size(); i++)
		{
			Entity entity = m_Scene->CreateEntity("Point Light");
			entity.AddComponent<Transform3DComponent>();
			entity.AddComponent<PointLightComponent>();
			m_LightEntities.push_back(entity);
		}
		for (size_t i = 0; i < lighting.SpotLights.size(); i++)
		{
			Entity entity = m_Scene->CreateEntity("Spot Light");
			entity.AddComponent<Transform3DComponent>();
			entity.AddComponent<SpotLightComponent>();
			m_LightEntities.push_back(entity);
		}
	}

	void LightingTest::UpdateLightEntities(const Lighting& lighting)
	{
		if (lighting.UsesDefaultLight)
			return;

		size_t next = 1; // after the ambient entity
		for (const PointLight& light : lighting.PointLights)
		{
			Entity entity = m_LightEntities[next++];
			entity.GetComponent<Transform3DComponent>().Position = light.Position;
			entity.GetComponent<PointLightComponent>() = PointLightComponent(light.Color, light.Intensity, light.Range);
		}
		for (const SpotLight& light : lighting.SpotLights)
		{
			Entity entity = m_LightEntities[next++];
			Transform3DComponent& transform = entity.GetComponent<Transform3DComponent>();
			transform.Position = light.Position;
			transform.Rotation = k_SpotEntityRotation;

			SpotLightComponent& spot = entity.GetComponent<SpotLightComponent>();
			spot = SpotLightComponent(light.Color, light.Intensity, light.Range);
			spot.Direction = glm::inverse(k_SpotEntityRotation) * light.Direction;
			spot.InnerConeAngle = light.InnerConeAngle;
			spot.OuterConeAngle = light.OuterConeAngle;
		}
	}

	void LightingTest::Cleanup()
	{
		Application::Get().GetRenderer3D().SetLightBudgetFade(0.0f);
		for (Material*& material : m_RowMaterials)
		{
			delete material;
			material = nullptr;
		}
		delete m_LampMaterial;
		delete m_CrateMaterial;
		m_LampMaterial = nullptr;
		m_CrateMaterial = nullptr;

		m_Alive.reset();
		for (Framebuffer*& target : m_FogTargets)
			DestroyAndDelete(target);
		m_FogTargets.clear();

		m_LightEntities.clear();
		m_LightEntitiesBuilt = false;
		m_FogEntity = {};
		delete m_Scene;
		m_Scene = nullptr;

		m_CheckSteps.clear();
		for (Renderer3D** renderer : { &m_CheckRenderer, &m_BudgetCheckRenderer })
		{
			if (*renderer)
			{
				(*renderer)->Shutdown();
				delete *renderer;
				*renderer = nullptr;
			}
		}
	}

	void LightingTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void LightingTest::ImGuiRender()
	{
		int mode = static_cast<int>(m_Mode);
		ImGui::RadioButton("Default light", &mode, static_cast<int>(Mode::DefaultLight));
		ImGui::RadioButton("Point and spot lights", &mode, static_cast<int>(Mode::PointAndSpot));
		ImGui::RadioButton("More lights than the budget", &mode, static_cast<int>(Mode::OverBudget));
		ImGui::RadioButton("Materials", &mode, static_cast<int>(Mode::Materials));
		m_Mode = static_cast<Mode>(mode);
		ImGui::Checkbox("Animate", &m_Animate);
		ImGui::Checkbox("Post chain (Soft tone curve)", &m_PostProcess);
		ImGui::Checkbox("Bloom (with the post chain)", &m_Bloom);
		ImGui::Checkbox("Ambient occlusion (with the post chain)", &m_AmbientOcclusion);
		ImGui::SliderFloat("Budget fade band", &m_BudgetFade, 0.0f, 2.0f);
		ImGui::Checkbox("Fog (into the clear colour)", &m_Fog);
		if (m_Mode == Mode::Materials)
			ImGui::Checkbox("Specular", &m_Specular);
		else
			ImGui::Checkbox("Lights as entities", &m_UseEntities);

		ImGui::Separator();
		const Renderer3D::Statistics& stats = Application::Get().GetRenderer3D().GetStatistics();
		ImGui::Text("Directional lights : %u", stats.DirectionalLights);
		ImGui::Text("Point/spot lights  : %u", stats.LocalLights);
		ImGui::Text("Out of view        : %u", stats.CulledLights);
		ImGui::Text("Over budget        : %u", stats.DroppedLights);
		ImGui::Text("Fogged             : %s", stats.Fogged ? "yes" : "no");

		ImGui::Separator();
		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		ImGui::Text("Checks");
		int failed = 0;
		for (const CheckResult& check : m_Checks)
		{
			failed += check.Passed ? 0 : 1;
			ImGui::TextColored(check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
				"[%s] %s", check.Passed ? "PASS" : "FAIL", check.Name.c_str());
		}
		if (m_NextCheckStep < m_CheckSteps.size())
			ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "running... (%d of %d)", static_cast<int>(m_NextCheckStep), static_cast<int>(m_CheckSteps.size()));
		else if (failed == 0)
			ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "all %d checks passed", static_cast<int>(m_Checks.size()));
		else
			ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%d of %d checks failed", failed, static_cast<int>(m_Checks.size()));
	}

}
