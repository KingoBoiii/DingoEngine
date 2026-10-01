#include "LightingTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

#include <cmath>
#include <format>

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

		std::string Counts(const Renderer3D::Statistics& stats)
		{
			return std::format("directional {}, local {}, culled {}, dropped {}",
				stats.DirectionalLights, stats.LocalLights, stats.CulledLights, stats.DroppedLights);
		}
	}

	void LightingTest::Check(bool condition, const std::string& name)
	{
		m_Checks.push_back({ name, condition });
		if (condition)
			DE_INFO("[PASS] {}", name);
		else
			DE_ERROR("[FAIL] {}", name);
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
	}

	void LightingTest::RunNextCheckStep()
	{
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

			m_Scene->SubmitLights(renderer);
			m_Scene->RenderEntities3D(renderer);
		}
		else
		{
			SubmitLights(renderer, lighting);
			DrawScene(renderer);
		}

		renderer.EndScene();

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
		for (Material*& material : m_RowMaterials)
		{
			delete material;
			material = nullptr;
		}
		delete m_LampMaterial;
		delete m_CrateMaterial;
		m_LampMaterial = nullptr;
		m_CrateMaterial = nullptr;

		m_LightEntities.clear();
		m_LightEntitiesBuilt = false;
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
