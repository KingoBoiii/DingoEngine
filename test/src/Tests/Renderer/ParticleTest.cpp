#include "ParticleTest.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

#include <cmath>
#include <cstring>
#include <format>

namespace Dingo
{

	namespace
	{
		constexpr uint32_t k_CheckWidth = 320;
		constexpr uint32_t k_CheckHeight = 240;
		constexpr uint32_t k_ParticleBytes = 48; // ParticleCommon.glsl's Particle
		const glm::vec3 k_PuffCenter{ 0.0f, 0.2f, 0.0f };

		Framebuffer* MakeTarget(const char* name)
		{
			return Framebuffer::Create(FramebufferParams()
				.SetDebugName(name)
				.SetWidth(static_cast<int32_t>(k_CheckWidth))
				.SetHeight(static_cast<int32_t>(k_CheckHeight))
				.SetEnableDepth(true)
				.AddAttachment({ TextureFormat::RGBA8_UNORM }));
		}

		PerspectiveCamera CheckCamera()
		{
			PerspectiveCamera camera(45.0f, static_cast<float>(k_CheckWidth) / static_cast<float>(k_CheckHeight), 0.1f, 100.0f);
			camera.SetPosition({ 0.0f, 0.6f, 3.0f });
			camera.SetTarget(k_PuffCenter);
			return camera;
		}

		glm::ivec2 ToPixel(const glm::mat4& viewProjection, const glm::vec3& point)
		{
			const glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
			const glm::vec2 ndc = glm::vec2(clip) / clip.w;
			return { static_cast<int>((ndc.x * 0.5f + 0.5f) * k_CheckWidth), static_cast<int>((0.5f - ndc.y * 0.5f) * k_CheckHeight) };
		}

		float GreenAt(const std::vector<uint8_t>& data, const glm::ivec2& pixel)
		{
			const glm::ivec2 p = glm::clamp(pixel, glm::ivec2(0), glm::ivec2(k_CheckWidth - 1, k_CheckHeight - 1));
			return static_cast<float>(data[(static_cast<size_t>(p.y) * k_CheckWidth + p.x) * 4 + 1]) / 255.0f;
		}

		ParticleEffectParams StillParticles(const char* name, float lifetime, uint32_t capacity)
		{
			return ParticleEffectParams()
				.SetDebugName(name)
				.SetRate(0.0f)
				.SetLifetime(lifetime, lifetime)
				.SetSpeed(0.0f, 0.0f)
				.SetCapacity(capacity);
		}
	}

	void ParticleTest::Initialize()
	{
		m_Checks.clear();
		m_Alive = std::make_shared<int>(0);
		m_CheckStep = 0;
		m_Time = 0.0f;
		m_NextBurst = 0.0f;

		const ApplicationCommandLineArgs& args = Application::Get().GetCommandLineArgs();
		if (auto mode = args.Get("particles"))
		{
			if (*mode == "burst")
				m_Mode = Mode::Burst;
			else if (*mode == "soft")
				m_Mode = Mode::Soft;
			else if (*mode == "budget")
				m_Mode = Mode::Budget;
			else
				m_Mode = Mode::Fountain;
		}

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.1f, 100.0f);
		m_Camera.SetPosition({ 0.0f, 2.5f, 7.0f });
		m_Camera.SetTarget({ 0.0f, 1.0f, 0.0f });

		m_FountainEffect = ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("ParticleTest fountain")
			.SetShape(ParticleShape::Cone, { 20.0f, 0.05f, 0.0f })
			.SetRate(800.0f)
			.SetLifetime(1.4f, 2.0f)
			.SetSpeed(4.0f, 6.0f)
			.SetGravity({ 0.0f, -9.8f, 0.0f })
			.SetStartSize(0.05f, 0.09f)
			.SetEndSize(0.4f)
			.SetColors({ { 0.0f, { 4.0f, 2.0f, 0.6f, 1.0f } }, { 0.6f, { 2.0f, 0.6f, 0.1f, 1.0f } }, { 1.0f, { 0.5f, 0.1f, 0.0f, 0.0f } } }));
		m_BurstEffect = ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("ParticleTest burst")
			.SetShape(ParticleShape::Sphere, { 0.1f, 0.0f, 0.0f })
			.SetRate(0.0f)
			.SetLifetime(0.6f, 1.2f)
			.SetSpeed(2.0f, 5.0f)
			.SetDrag(1.5f)
			.SetGravity({ 0.0f, -2.0f, 0.0f })
			.SetStartSize(0.04f, 0.08f)
			.SetColors({ { 0.0f, { 1.0f, 3.0f, 6.0f, 1.0f } }, { 1.0f, { 0.2f, 0.4f, 1.0f, 0.0f } } })
			.SetCapacity(700));
		m_SmokeEffect = ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("ParticleTest smoke")
			.SetShape(ParticleShape::Cone, { 15.0f, 0.3f, 0.0f })
			.SetRate(30.0f)
			.SetLifetime(3.0f, 4.0f)
			.SetSpeed(0.2f, 0.5f)
			.SetNoise(0.8f, 0.7f)
			.SetStartSize(0.6f, 1.0f)
			.SetEndSize(2.5f)
			.SetSpin(-20.0f, 20.0f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.5f, 0.5f, 0.55f, 0.0f } }, { 0.15f, { 0.5f, 0.5f, 0.55f, 0.5f } }, { 1.0f, { 0.6f, 0.6f, 0.6f, 0.0f } } })
			.SetSoftDistance(0.5f));
		m_BudgetEffect = ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("ParticleTest budget")
			.SetShape(ParticleShape::Box, { 3.0f, 0.1f, 2.0f })
			.SetRate(20000.0f)
			.SetLifetime(1.0f, 1.5f)
			.SetSpeed(0.5f, 2.0f)
			.SetNoise(2.0f, 1.5f)
			.SetStartSize(0.02f, 0.04f)
			.SetColors({ { 0.0f, { 0.4f, 1.0f, 0.6f, 1.0f } }, { 1.0f, { 0.1f, 0.3f, 1.0f, 0.0f } } }));

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		m_Fountain = renderer.CreateParticleEmitter(m_FountainEffect);
		m_Burst = renderer.CreateParticleEmitter(m_BurstEffect);
		m_Smoke = renderer.CreateParticleEmitter(m_SmokeEffect);
		m_BudgetEmitter = renderer.CreateParticleEmitter(m_BudgetEffect);

		m_CheckTarget = MakeTarget("ParticleTest check");
		m_SoftTarget = MakeTarget("ParticleTest soft");
		m_HardTarget = MakeTarget("ParticleTest hard");

		m_CheckEffects.emplace_back(ParticleEffect::Create(StillParticles("ParticleTest scene", 5.0f, 32)));
		m_Scene = new Scene("Particle Test");
		m_Scene->SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		Entity camera = m_Scene->CreateEntity("Camera");
		camera.AddComponent<Transform3DComponent>().Position = { 0.0f, 1.0f, 5.0f };
		CameraComponent& cameraComponent = camera.AddComponent<CameraComponent>();
		cameraComponent.Type = CameraComponent::ProjectionType::Perspective;
		m_SceneEmitter = m_Scene->CreateEntity("Emitter");
		m_SceneEmitter.AddComponent<Transform3DComponent>();
		m_SceneEmitter.AddComponent<ParticleEmitterComponent>(m_CheckEffects.back().get());
	}

	void ParticleTest::CountAlive(Renderer3D& renderer, const ParticleEmitter& emitter, std::function<void(uint32_t, float)> done)
	{
		GraphicsBuffer* pool = renderer.GetParticlePool();
		if (!pool || emitter.GetCapacity() == 0)
		{
			done(0, 0.0f);
			return;
		}

		const std::weak_ptr<int> alive = m_Alive;
		const uint32_t capacity = emitter.GetCapacity();
		pool->ReadBack([alive, capacity, done = std::move(done)](const std::vector<uint8_t>& bytes)
		{
			if (alive.expired())
				return;
			uint32_t count = 0;
			float worst = 0.0f;
			for (uint32_t i = 0; i < capacity && (i + 1) * k_ParticleBytes <= bytes.size(); ++i)
			{
				float particle[12];
				std::memcpy(particle, bytes.data() + static_cast<size_t>(i) * k_ParticleBytes, sizeof(particle));
				const float age = particle[3];
				const float life = particle[7];
				if (life > 0.0f)
				{
					++count;
					worst = (std::max)(worst, age / life);
				}
			}
			done(count, worst);
		}, static_cast<uint64_t>(emitter.GetPoolOffset()) * k_ParticleBytes, static_cast<uint64_t>(capacity) * k_ParticleBytes);
	}

	void ParticleTest::DrawCheckScene(Framebuffer* target, std::initializer_list<std::pair<ParticleEmitter*, float>> emitters, bool post, const glm::mat4& emitterTransform)
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);

		const PerspectiveCamera camera = CheckCamera();
		PostProcessSettings settings;
		settings.Enabled = post;
		settings.Tone.Operator = ToneMapOperator::None;
		PostProcessStack& stack = Renderer::GetPostProcessStack();
		stack.Begin(settings, camera.GetProjectionMatrix());

		m_CheckRenderer->BeginScene(camera);
		m_CheckRenderer->Clear({ 0.0f, 0.0f, 0.0f, 1.0f });
		m_CheckRenderer->SetAmbientLight(glm::vec3(1.0f), 1.0f);
		m_CheckRenderer->SubmitMesh(m_CheckRenderer->GetBoxMesh(), glm::scale(glm::translate(glm::mat4(1.0f), { 0.0f, -0.05f, 0.0f }), { 30.0f, 0.1f, 30.0f }), { 0.0f, 0.0f, 0.0f, 1.0f });
		for (const auto& [emitter, deltaTime] : emitters)
			m_CheckRenderer->SubmitParticles(*emitter, emitterTransform, deltaTime);
		m_CheckRenderer->EndScene();

		stack.End();
		Renderer::SetRenderTarget(previous);
	}

	void ParticleTest::RunCheckStep()
	{
		const int step = m_CheckStep++;
		if (step == 0)
		{
			m_CheckRenderer = Renderer3D::Create();
			m_CheckEffects.emplace_back(ParticleEffect::Create(StillParticles("ParticleTest whole pool", 10.0f, Renderer3DCapabilities().MaxParticles)));
			ParticleEffect* whole = m_CheckEffects.back().get();
			m_CheckEffects.emplace_back(ParticleEffect::Create(StillParticles("ParticleTest no room", 10.0f, 16)));
			ParticleEffect* noRoom = m_CheckEffects.back().get();

			m_Whole = m_CheckRenderer->CreateParticleEmitter(whole);
			m_NoRoom = m_CheckRenderer->CreateParticleEmitter(noRoom);
			Check(m_Whole->GetCapacity() == m_CheckRenderer->GetParticlePoolCapacity() && m_NoRoom->GetCapacity() == 0,
				std::format("an emitter takes the whole pool of {} and another finds no room ({}, {})", m_CheckRenderer->GetParticlePoolCapacity(), m_Whole->GetCapacity(), m_NoRoom->GetCapacity()));

			m_Whole->Emit(m_Whole->GetCapacity());
			DrawCheckScene(m_CheckTarget, { { m_Whole.get(), 1.0f / 60.0f }, { m_NoRoom.get(), 1.0f / 60.0f } }, false);
			const Renderer3D::Statistics stats = m_CheckRenderer->GetStatistics();
			const uint32_t expected = m_Whole->GetCapacity();
			Check(stats.ParticlesSpawned == expected && stats.ParticleEmitters == 1, std::format("{} particles spawn in one step ({} spawned, {} emitters drawn)", expected, stats.ParticlesSpawned, stats.ParticleEmitters));
			CountAlive(*m_CheckRenderer, *m_Whole, [this, expected](uint32_t alive, float)
			{
				Check(alive == expected, std::format("all {} particles of a full pool are alive ({} alive)", expected, alive));
			});
			return;
		}

		if (step == 1)
		{
			m_Whole.reset();
			m_NoRoom.reset();
			Check(m_CheckRenderer->GetParticlePoolUsed() == 0, std::format("released emitters return their rings to the pool ({} in use)", m_CheckRenderer->GetParticlePoolUsed()));

			m_CheckEffects.emplace_back(ParticleEffect::Create(StillParticles("ParticleTest short", 0.5f, 128)));
			m_Short = m_CheckRenderer->CreateParticleEmitter(m_CheckEffects.back().get());
			m_CheckEffects.emplace_back(ParticleEffect::Create(StillParticles("ParticleTest small", 5.0f, 64)));
			m_Small = m_CheckRenderer->CreateParticleEmitter(m_CheckEffects.back().get());
			m_Short->Emit(100);
			m_Small->Emit(100);
			DrawCheckScene(m_CheckTarget, { { m_Short.get(), 1.0f / 60.0f }, { m_Small.get(), 1.0f / 60.0f } }, false);
			const Renderer3D::Statistics stats = m_CheckRenderer->GetStatistics();
			Check(stats.ParticlesSpawned == 164 && stats.DroppedParticleSpawns == 36,
				std::format("a ring of 64 asked for 100 spawns 64 and drops 36 ({} spawned, {} dropped)", stats.ParticlesSpawned, stats.DroppedParticleSpawns));
			CountAlive(*m_CheckRenderer, *m_Short, [this](uint32_t alive, float) { Check(alive == 100, std::format("a burst of 100 gives 100 live particles ({})", alive)); });
			CountAlive(*m_CheckRenderer, *m_Small, [this](uint32_t alive, float) { Check(alive == 64, std::format("the full ring keeps 64 live particles ({})", alive)); });
			return;
		}

		if (step == 2)
		{
			DrawCheckScene(m_CheckTarget, { { m_Short.get(), 0.6f }, { m_Small.get(), 1.0f / 60.0f } }, false);
			CountAlive(*m_CheckRenderer, *m_Short, [this](uint32_t alive, float) { Check(alive == 0, std::format("past their 0.5 s lifetime none are left ({} alive)", alive)); });
			CountAlive(*m_CheckRenderer, *m_Small, [this](uint32_t alive, float worst)
			{
				Check(alive == 64 && worst < 1.0f, std::format("the others age without outliving their lifetime ({} alive, oldest at {:.3f} of its life)", alive, worst));
			});
			return;
		}

		if (step == 3)
		{
			m_Scene->EmitParticles(m_SceneEmitter, 10);
			Application::Get().GetSceneRenderer().Render(*m_Scene, m_CheckTarget);
			ParticleEmitter* emitter = m_Scene->GetParticleEmitter(m_SceneEmitter);
			Check(emitter && emitter->GetCapacity() == 32, "a ParticleEmitterComponent gets its emitter on its first draw");
			if (emitter)
				CountAlive(Application::Get().GetRenderer3D(), *emitter, [this](uint32_t alive, float) { Check(alive == 10, std::format("Scene::EmitParticles before the first draw lands in the component's emitter ({} alive)", alive)); });
			return;
		}

		if (step == 4)
		{
			ParticleEffectParams puff = StillParticles("ParticleTest soft", 100.0f, 4)
				.SetBurstOnPlay(1)
				.SetStartSize(2.0f, 2.0f)
				.SetColors({ { 0.0f, glm::vec4(1.0f) } })
				.SetSoftDistance(0.5f);
			m_CheckEffects.emplace_back(ParticleEffect::Create(puff));
			m_SoftPuff = m_CheckRenderer->CreateParticleEmitter(m_CheckEffects.back().get());
			m_CheckEffects.emplace_back(ParticleEffect::Create(puff.SetDebugName("ParticleTest hard").SetSoftDistance(0.0f)));
			m_HardPuff = m_CheckRenderer->CreateParticleEmitter(m_CheckEffects.back().get());

			const glm::mat4 at = glm::translate(glm::mat4(1.0f), k_PuffCenter);
			DrawCheckScene(m_SoftTarget, { { m_SoftPuff.get(), 1.0f / 60.0f } }, true, at);
			DrawCheckScene(m_HardTarget, { { m_HardPuff.get(), 1.0f / 60.0f } }, true, at);

			const glm::mat4 viewProjection = CheckCamera().GetViewProjectionMatrix();
			const glm::ivec2 nearFloor = ToPixel(viewProjection, { 0.0f, 0.05f, 0.0f });
			const glm::ivec2 high = ToPixel(viewProjection, { 0.0f, 0.5f, 0.0f });
			const std::weak_ptr<int> alive = m_Alive;
			m_HardTarget->GetAttachment(0)->ReadPixels([this, alive](const TexturePixels& pixels)
			{
				if (!alive.expired())
					m_HardPixels = pixels.Data;
			});
			m_SoftTarget->GetAttachment(0)->ReadPixels([this, alive, nearFloor, high](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				if (pixels.Data.size() != m_HardPixels.size() || pixels.Data.empty())
				{
					Check(false, "the soft and hard particles read back");
					return;
				}
				const float softLow = GreenAt(pixels.Data, nearFloor);
				const float hardLow = GreenAt(m_HardPixels, nearFloor);
				const float softHigh = GreenAt(pixels.Data, high);
				const float hardHigh = GreenAt(m_HardPixels, high);
				Check(hardLow > 0.2f && softLow > 0.0f && softLow < 0.75f * hardLow,
					std::format("a soft particle fades where it meets the floor ({:.3f} against a hard one's {:.3f})", softLow, hardLow));
				Check(std::abs(softHigh - hardHigh) <= 1.01f / 255.0f && hardHigh > 0.2f,
					std::format("and draws like a hard one away from it ({:.3f} against {:.3f})", softHigh, hardHigh));
			});
		}
	}

	void ParticleTest::DrawLive(float deltaTime)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		PostProcessSettings settings;
		settings.Enabled = m_PostChain;
		settings.Bloom.Enabled = m_Mode == Mode::Fountain || m_Mode == Mode::Burst;
		PostProcessStack& post = Renderer::GetPostProcessStack();
		post.Begin(settings, m_Camera.GetProjectionMatrix());

		renderer.BeginScene(m_Camera);
		renderer.Clear({ 0.02f, 0.02f, 0.03f, 1.0f });
		DirectionalLight moon;
		moon.Direction = { -0.3f, -1.0f, -0.4f };
		moon.Intensity = 0.25f;
		renderer.SubmitLight(moon);
		renderer.SetAmbientLight({ 0.5f, 0.6f, 1.0f }, 0.1f);
		renderer.SubmitMesh(renderer.GetBoxMesh(), glm::scale(glm::translate(glm::mat4(1.0f), { 0.0f, -0.05f, 0.0f }), { 20.0f, 0.1f, 20.0f }), { 0.5f, 0.5f, 0.5f, 1.0f });

		switch (m_Mode)
		{
			case Mode::Fountain:
				renderer.SubmitParticles(*m_Fountain, glm::mat4(1.0f), deltaTime);
				break;
			case Mode::Burst:
				if (m_Time >= m_NextBurst)
				{
					const float angle = m_Time * 2.3f;
					m_Burst->EmitAt({ 2.0f * std::cos(angle), 1.0f + 0.5f * std::sin(m_Time * 1.7f), 1.5f * std::sin(angle) }, 300);
					m_NextBurst = m_Time + 1.0f;
				}
				renderer.SubmitParticles(*m_Burst, glm::mat4(1.0f), deltaTime);
				break;
			case Mode::Soft:
			{
				ParticleEffectParams params = m_SmokeEffect->GetParams();
				params.SoftDistance = m_SoftEdges ? 0.5f : 0.0f;
				m_SmokeEffect->SetParams(params);
				renderer.SubmitParticles(*m_Smoke, glm::translate(glm::mat4(1.0f), { 0.0f, 0.1f, 0.0f }), deltaTime);
				break;
			}
			case Mode::Budget:
				renderer.SubmitParticles(*m_BudgetEmitter, glm::translate(glm::mat4(1.0f), { 0.0f, 0.5f, 0.0f }), deltaTime);
				break;
		}

		renderer.EndScene();
		post.End();
	}

	void ParticleTest::Update(float deltaTime)
	{
		if (m_CheckStep < 5 && !Renderer::IsFrameSkipped())
			RunCheckStep();

		m_Time += deltaTime;
		DrawLive(deltaTime);
	}

	void ParticleTest::Cleanup()
	{
		m_Alive.reset();
		m_Fountain.reset();
		m_Burst.reset();
		m_Smoke.reset();
		m_BudgetEmitter.reset();
		m_Whole.reset();
		m_NoRoom.reset();
		m_Short.reset();
		m_Small.reset();
		m_SoftPuff.reset();
		m_HardPuff.reset();

		delete m_Scene;
		m_Scene = nullptr;
		m_SceneEmitter = {};
		if (m_CheckRenderer)
		{
			m_CheckRenderer->Shutdown();
			delete m_CheckRenderer;
			m_CheckRenderer = nullptr;
		}
		m_CheckEffects.clear();

		delete m_FountainEffect;
		delete m_BurstEffect;
		delete m_SmokeEffect;
		delete m_BudgetEffect;
		m_FountainEffect = m_BurstEffect = m_SmokeEffect = m_BudgetEffect = nullptr;

		DestroyAndDelete(m_CheckTarget);
		DestroyAndDelete(m_SoftTarget);
		DestroyAndDelete(m_HardTarget);
		m_HardPixels.clear();
		m_Checks.clear();
	}

	void ParticleTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void ParticleTest::ImGuiRender()
	{
		int mode = static_cast<int>(m_Mode);
		ImGui::RadioButton("Fountain", &mode, static_cast<int>(Mode::Fountain));
		ImGui::SameLine();
		ImGui::RadioButton("Burst", &mode, static_cast<int>(Mode::Burst));
		ImGui::SameLine();
		ImGui::RadioButton("Soft smoke", &mode, static_cast<int>(Mode::Soft));
		ImGui::SameLine();
		ImGui::RadioButton("Budget", &mode, static_cast<int>(Mode::Budget));
		m_Mode = static_cast<Mode>(mode);
		ImGui::Checkbox("Post chain (bloom for sparks)", &m_PostChain);
		ImGui::Checkbox("Soft edges", &m_SoftEdges);

		const Renderer3D& renderer = Application::Get().GetRenderer3D();
		const Renderer3D::Statistics& stats = renderer.GetStatistics();
		ImGui::Text("Pool %u / %u, %u emitters, %u slots, %u spawned", renderer.GetParticlePoolUsed(), renderer.GetParticlePoolCapacity(), stats.ParticleEmitters, stats.ParticleSlots, stats.ParticlesSpawned);

		GraphicsTest::ImGuiRender();

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
