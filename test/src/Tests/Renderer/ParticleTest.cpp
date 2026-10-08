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

		constexpr const char* k_FoxPath = "assets/models/Fox/Fox.gltf";
		constexpr float k_FoxLength = 1.6f;
		constexpr uint32_t k_DustPerStep = 6;
		constexpr int k_BlendFrames = 120;
		constexpr int k_SurveyFrames = 180;
		constexpr float k_EventStep = 1.0f / 60.0f;

		// Counts the footfalls the Fox's script hears, to hold the bursts to.
		class FootfallCounter : public ScriptableEntity
		{
		public:
			explicit FootfallCounter(uint32_t* count) : m_Count(count) {}

			void OnAnimationEvent(const AnimationEvent& event) override
			{
				if (event.Type == AnimationEventType::Instant && event.Name.starts_with("step_"))
					++*m_Count;
			}

		private:
			uint32_t* m_Count = nullptr;
		};

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
			else if (*mode == "events")
				m_Mode = Mode::Events;
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
			.SetStartRotation(0.0f, 360.0f)
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

		// A duplicated rig's binding to its own socketed emitter follows the copy; one to an emitter
		// outside the rig still names that one.
		{
			Entity rig = m_Scene->CreateEntity("Duplicate rig");
			rig.AddComponent<Transform3DComponent>();
			Entity inner = m_Scene->CreateEntity("Duplicate rig emitter");
			inner.AddComponent<Transform3DComponent>();
			inner.SetParent(rig);
			rig.AddComponent<ParticleEventComponent>()
				.Bind("step", inner.GetUUID(), 1)
				.Bind("step", m_SceneEmitter.GetUUID(), 1);

			Entity copy = m_Scene->DuplicateEntity(rig);
			const std::vector<Entity> children = copy.GetChildren();
			const std::vector<ParticleEventComponent::Binding>& bindings = copy.GetComponent<ParticleEventComponent>().Bindings;
			Check(children.size() == 1 && bindings.size() == 2 && bindings[0].Emitter == children[0].GetUUID() && bindings[1].Emitter == m_SceneEmitter.GetUUID()
					&& rig.GetComponent<ParticleEventComponent>().Bindings[0].Emitter == inner.GetUUID(),
				"a duplicated entity's particle binding follows its own copied emitter and keeps one outside it");
			m_Scene->DestroyEntity(copy);
			m_Scene->DestroyEntity(rig);
		}

		BuildEventScene();
	}

	void ParticleTest::BuildEventScene()
	{
		m_EventStep = 0;
		m_Footfalls = 0;
		m_DustSpawned = 0;
		m_MotesSpawned = 0;
		m_MotesPlayedOpen = false;
		m_MotesStoppedClosed = false;

		m_Fox = Model::LoadFromFile(k_FoxPath);
		if (!m_Fox || !m_Fox->GetSkeleton())
			return;

		glm::vec3 low((std::numeric_limits<float>::max)());
		glm::vec3 high((std::numeric_limits<float>::lowest)());
		for (const SubMesh& submesh : m_Fox->GetSubMeshes())
		{
			for (const MeshVertex& vertex : submesh.MeshData->GetVertices())
			{
				low = (glm::min)(low, vertex.Position);
				high = (glm::max)(high, vertex.Position);
			}
		}
		const glm::vec3 extent = high - low;
		const float scale = k_FoxLength / (std::max)({ extent.x, extent.y, extent.z });

		m_DustEffect = ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("ParticleTest footfall dust")
			.SetShape(ParticleShape::Cone, { 70.0f, 0.1f, 0.0f })
			.SetRate(0.0f)
			.SetLifetime(0.5f, 0.9f)
			.SetSpeed(0.3f, 0.7f)
			.SetDrag(2.0f)
			.SetGravity({ 0.0f, 0.2f, 0.0f })
			.SetStartSize(0.08f, 0.14f)
			.SetEndSize(2.5f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.6f, 0.55f, 0.45f, 0.6f } }, { 1.0f, { 0.6f, 0.55f, 0.45f, 0.0f } } }));
		m_MoteEffect = ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("ParticleTest look motes")
			.SetShape(ParticleShape::Sphere, { 0.4f, 0.0f, 0.0f })
			.SetRate(120.0f)
			.SetLifetime(0.6f, 1.0f)
			.SetSpeed(0.1f, 0.3f)
			.SetStartSize(0.03f, 0.05f)
			.SetColors({ { 0.0f, { 2.0f, 2.5f, 4.0f, 1.0f } }, { 1.0f, { 0.5f, 0.8f, 2.0f, 0.0f } } }));

		m_EventScene = new Scene("Particle Test: events");
		m_EventScene->SetClearColor({ 0.05f, 0.05f, 0.07f, 1.0f });

		Entity camera = m_EventScene->CreateEntity("Camera");
		Transform3DComponent& cameraTransform = camera.AddComponent<Transform3DComponent>();
		cameraTransform.Position = { -2.2f, 1.2f, 2.2f };
		cameraTransform.Rotation = glm::quatLookAt(glm::normalize(glm::vec3(0.0f, 0.4f, 0.0f) - cameraTransform.Position), glm::vec3(0.0f, 1.0f, 0.0f));
		camera.AddComponent<CameraComponent>().Type = CameraComponent::ProjectionType::Perspective;

		Entity floor = m_EventScene->CreateEntity("Floor");
		floor.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -0.05f, 0.0f }, { 10.0f, 0.1f, 10.0f }));
		floor.AddComponent<MeshRendererComponent>(MeshRendererComponent(Application::Get().GetRenderer3D().GetBoxMesh(), { 0.4f, 0.4f, 0.4f, 1.0f }));

		m_EventFox = m_EventScene->CreateEntity("Fox");
		m_EventFox.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -low.y * scale, 0.0f }, glm::vec3(scale)));
		m_EventFox.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_Fox));
		m_EventFox.AddComponent<AnimatorComponent>(AnimatorComponent("Walk"));
		m_EventFox.AddScript<FootfallCounter>(&m_Footfalls);

		m_Dust = m_EventScene->CreateEntity("Dust");
		m_Dust.AddComponent<Transform3DComponent>();
		m_Dust.AddComponent<ParticleEmitterComponent>(m_DustEffect);
		m_Motes = m_EventScene->CreateEntity("Motes");
		m_Motes.AddComponent<Transform3DComponent>().Position = { 0.0f, 1.0f, 0.0f };
		m_Motes.AddComponent<ParticleEmitterComponent>(m_MoteEffect).Playing = false;

		ParticleEventComponent& events = m_EventFox.AddComponent<ParticleEventComponent>();
		for (const char* step : { "step_fl", "step_fr", "step_bl", "step_br" })
			events.Bind(step, m_Dust.GetUUID(), k_DustPerStep);
		events.BindRange("look", m_Motes.GetUUID());

		m_EventScene->OnStart();
		if (Animator* animator = m_EventScene->GetAnimator(m_EventFox))
		{
			const AnimationClip* walk = m_Fox->FindAnimation("Walk");
			const AnimationClip* run = m_Fox->FindAnimation("Run");
			if (walk && run)
				animator->Play(AnimationState::Blend1D("Speed", { { 0.0f, walk }, { 1.0f, run } }));
		}
	}

	void ParticleTest::RunEventStep()
	{
		const int step = m_EventStep++;
		Animator* animator = m_EventScene->GetAnimator(m_EventFox);
		if (!animator)
			return;

		if (step < k_BlendFrames)
		{
			animator->SetFloat("Speed", static_cast<float>(step) / static_cast<float>(k_BlendFrames - 1));
		}
		else if (step == k_BlendFrames)
		{
			Check(m_Footfalls > 0 && m_DustSpawned == m_Footfalls * k_DustPerStep,
				std::format("across a Walk-to-Run blend every footfall bursts once ({} footfalls, {} dust particles, {} expected)", m_Footfalls, m_DustSpawned, m_Footfalls * k_DustPerStep));
			if (const AnimationClip* survey = m_Fox->FindAnimation("Survey"))
				animator->Play(survey);
		}

		m_EventScene->OnUpdate(k_EventStep);
		const Renderer3D& renderer = Application::Get().GetRenderer3D();
		Application::Get().GetSceneRenderer().Render(*m_EventScene, m_CheckTarget);

		const bool motesPlaying = m_Motes.GetComponent<ParticleEmitterComponent>().Playing;
		if (step < k_BlendFrames)
		{
			m_DustSpawned += renderer.GetStatistics().ParticlesSpawned;
			return;
		}

		// Survey's "look" spans 0.90 to 2.50 s; its emitter is the only one that spawns at a rate.
		const float surveyTime = static_cast<float>(step - k_BlendFrames + 1) * k_EventStep;
		if (surveyTime > 1.2f && surveyTime < 2.3f)
			m_MotesPlayedOpen = m_MotesPlayedOpen || motesPlaying;
		if (surveyTime > 2.7f)
			m_MotesStoppedClosed = !motesPlaying;
		if (motesPlaying)
			m_MotesSpawned += renderer.GetStatistics().ParticlesSpawned;

		if (step + 1 == k_BlendFrames + k_SurveyFrames)
		{
			Check(m_MotesPlayedOpen && m_MotesStoppedClosed && m_MotesSpawned > 0,
				std::format("Survey's \"look\" range plays its emitter while open and stops it on RangeEnd (played {}, stopped {}, {} spawned)", m_MotesPlayedOpen, m_MotesStoppedClosed, m_MotesSpawned));
		}
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
			m_FirstSceneEmitter = emitter;
			Check(emitter && emitter->GetCapacity() == 32, "a ParticleEmitterComponent gets its emitter on its first draw");
			if (emitter)
				CountAlive(Application::Get().GetRenderer3D(), *emitter, [this](uint32_t alive, float) { Check(alive == 10, std::format("Scene::EmitParticles before the first draw lands in the component's emitter ({} alive)", alive)); });
			return;
		}

		if (step == 4)
		{
			// A second renderer drawing the same scene runs an emitter of its own and leaves the first
			// one's particles alone.
			const uint32_t usedBefore = m_CheckRenderer->GetParticlePoolUsed();
			m_Scene->EmitParticles(m_SceneEmitter, 5);
			for (int pass = 0; pass < 2; ++pass)
			{
				Framebuffer* previous = Renderer::GetRenderTarget();
				Renderer::SetRenderTarget(m_CheckTarget);
				m_CheckRenderer->BeginScene(CheckCamera());
				m_Scene->RenderEntities3D(*m_CheckRenderer);
				m_CheckRenderer->EndScene();
				Renderer::SetRenderTarget(previous);
				Application::Get().GetSceneRenderer().Render(*m_Scene, m_CheckTarget);
			}
			ParticleEmitter* sceneEmitter = m_Scene->GetParticleEmitter(m_SceneEmitter);
			const uint32_t usedAfter = m_CheckRenderer->GetParticlePoolUsed();
			Check(sceneEmitter && sceneEmitter == m_FirstSceneEmitter && usedAfter == usedBefore + 32,
				std::format("a scene drawn by two renderers keeps its first emitter and makes one more ({} pool slots taken)", usedAfter - usedBefore));
			if (sceneEmitter && sceneEmitter == m_FirstSceneEmitter)
				CountAlive(Application::Get().GetRenderer3D(), *sceneEmitter, [this](uint32_t alive, float) { Check(alive == 15, std::format("and the first emitter keeps its particles and gets the new burst ({} alive)", alive)); });

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
			return;
		}

		if (step == 5)
		{
			// A 2x2 flipbook laid out as a loaded file is, bottom row first: red and green on top, then
			// blue and white. Frame 0 is the top-left cell and frame 2 the bottom-left one.
			const uint8_t texels[] = {
				0, 0, 255, 255,   255, 255, 255, 255,
				255, 0, 0, 255,   0, 255, 0, 255 };
			m_FlipbookTexture = Texture::CreateFromData(2, 2, texels, TextureFormat::RGBA, "ParticleTest flipbook");
			m_CheckEffects.emplace_back(ParticleEffect::Create(StillParticles("ParticleTest flipbook", 100.0f, 4)
				.SetBurstOnPlay(1)
				.SetStartSize(2.0f, 2.0f)
				.SetColors({ { 0.0f, glm::vec4(1.0f) } })
				.SetTexture(m_FlipbookTexture, 2, 2)));
			m_Flipbook = m_CheckRenderer->CreateParticleEmitter(m_CheckEffects.back().get());

			const glm::mat4 at = glm::translate(glm::mat4(1.0f), k_PuffCenter);
			DrawCheckScene(m_CheckTarget, { { m_Flipbook.get(), 1.0f / 60.0f } }, false, at);
			DrawCheckScene(m_HardTarget, { { m_Flipbook.get(), 60.0f } }, false, at);

			const glm::ivec2 center = ToPixel(CheckCamera().GetViewProjectionMatrix(), k_PuffCenter);
			const std::weak_ptr<int> alive = m_Alive;
			auto check = [this, alive, center](const char* what, glm::vec3 expected)
			{
				return [this, alive, center, what, expected](const TexturePixels& pixels)
				{
					if (alive.expired())
						return;
					if (pixels.Data.empty())
					{
						Check(false, std::format("the {} reads back", what));
						return;
					}
					const glm::ivec2 p = glm::clamp(center, glm::ivec2(0), glm::ivec2(k_CheckWidth - 1, k_CheckHeight - 1));
					const uint8_t* texel = &pixels.Data[(static_cast<size_t>(p.y) * k_CheckWidth + p.x) * 4];
					const glm::vec3 color = glm::vec3(texel[0], texel[1], texel[2]) / 255.0f;
					const float error = glm::length(color - expected);
					Check(error < 0.15f, std::format("the {} ({:.2f}, {:.2f}, {:.2f})", what, color.r, color.g, color.b));
				};
			};
			m_CheckTarget->GetAttachment(0)->ReadPixels(check("flipbook starts on its top-left frame, red", { 1.0f, 0.0f, 0.0f }));
			m_HardTarget->GetAttachment(0)->ReadPixels(check("flipbook's third frame is the bottom-left one, blue", { 0.0f, 0.0f, 1.0f }));

			// A sprite white on top and black below, bottom row first as loaded, with the default
			// StartRotation: it stands upright.
			const uint8_t halves[] = { 0, 0, 0, 255,   255, 255, 255, 255 };
			m_UprightTexture = Texture::CreateFromData(1, 2, halves, TextureFormat::RGBA, "ParticleTest upright");
			m_CheckEffects.emplace_back(ParticleEffect::Create(StillParticles("ParticleTest upright", 100.0f, 4)
				.SetBurstOnPlay(1)
				.SetStartSize(2.0f, 2.0f)
				.SetColors({ { 0.0f, glm::vec4(1.0f) } })
				.SetTexture(m_UprightTexture)));
			m_Upright = m_CheckRenderer->CreateParticleEmitter(m_CheckEffects.back().get());
			const glm::vec3 uprightCenter = k_PuffCenter + glm::vec3(0.0f, 0.5f, 0.0f);
			DrawCheckScene(m_SoftTarget, { { m_Upright.get(), 1.0f / 60.0f } }, false, glm::translate(glm::mat4(1.0f), uprightCenter));

			const PerspectiveCamera camera = CheckCamera();
			const glm::vec3 forward = glm::normalize(k_PuffCenter - camera.GetPosition());
			const glm::vec3 up = glm::normalize(glm::cross(glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f))), forward));
			const glm::ivec2 top = ToPixel(camera.GetViewProjectionMatrix(), uprightCenter + up * 0.5f);
			const glm::ivec2 bottom = ToPixel(camera.GetViewProjectionMatrix(), uprightCenter - up * 0.5f);
			m_SoftTarget->GetAttachment(0)->ReadPixels([this, alive, top, bottom](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				if (pixels.Data.empty())
				{
					Check(false, "the upright sprite reads back");
					return;
				}
				const float above = GreenAt(pixels.Data, top);
				const float below = GreenAt(pixels.Data, bottom);
				Check(above > 0.85f && below < 0.15f, std::format("a sprite stands upright: its top half on top ({:.2f} above, {:.2f} below)", above, below));
			});
			return;
		}

		if (step == 6)
		{
			// One emitter in ten scenes of a frame, nine looking away: the tenth draws with its own
			// camera, past the 8 writes a frame a default material uniform buffer holds on Vulkan.
			m_CheckEffects.emplace_back(ParticleEffect::Create(StillParticles("ParticleTest scenes", 100.0f, 4)
				.SetBurstOnPlay(1)
				.SetStartSize(1.0f, 1.0f)
				.SetColors({ { 0.0f, glm::vec4(1.0f) } })));
			m_ManyScenes = m_CheckRenderer->CreateParticleEmitter(m_CheckEffects.back().get());
			const glm::mat4 at = glm::translate(glm::mat4(1.0f), k_PuffCenter);
			PerspectiveCamera away = CheckCamera();
			away.SetTarget(away.GetPosition() + (away.GetPosition() - k_PuffCenter));
			for (int scene = 0; scene < 10; ++scene)
			{
				const bool last = scene == 9;
				Framebuffer* previous = Renderer::GetRenderTarget();
				Renderer::SetRenderTarget(last ? m_CheckTarget : m_HardTarget);
				m_CheckRenderer->BeginScene(last ? CheckCamera() : away);
				m_CheckRenderer->Clear({ 0.0f, 0.0f, 0.0f, 1.0f });
				m_CheckRenderer->SubmitParticles(*m_ManyScenes, at, scene == 0 ? 1.0f / 60.0f : 0.0f);
				m_CheckRenderer->EndScene();
				Renderer::SetRenderTarget(previous);
			}
			const glm::ivec2 center = ToPixel(CheckCamera().GetViewProjectionMatrix(), k_PuffCenter);
			const std::weak_ptr<int> alive = m_Alive;
			m_CheckTarget->GetAttachment(0)->ReadPixels([this, alive, center](const TexturePixels& pixels)
			{
				if (alive.expired())
					return;
				const float green = pixels.Data.empty() ? 0.0f : GreenAt(pixels.Data, center);
				Check(green > 0.3f, std::format("the tenth scene of a frame draws its particles with its own camera ({:.2f} at the particle)", green));
			});
		}
	}

	void ParticleTest::DrawLive(float deltaTime)
	{
		if (m_Mode == Mode::Events)
		{
			if (!m_EventScene)
				return;
			if (m_EventStep >= k_BlendFrames + k_SurveyFrames)
				m_EventScene->OnUpdate(deltaTime);
			Application::Get().GetSceneRenderer().Render(*m_EventScene);
			return;
		}

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
		if (m_CheckStep < 7 && !Renderer::IsFrameSkipped())
			RunCheckStep();
		else if (m_EventScene && m_EventStep < k_BlendFrames + k_SurveyFrames && !Renderer::IsFrameSkipped())
			RunEventStep();
		else if (!m_EventScene && m_EventStep == 0)
		{
			m_EventStep = 1;
			Check(false, "Fox.gltf loads for the animation event checks");
		}

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
		m_Flipbook.reset();
		m_Upright.reset();
		m_ManyScenes.reset();

		delete m_Scene;
		m_Scene = nullptr;
		m_SceneEmitter = {};
		delete m_EventScene;
		m_EventScene = nullptr;
		m_EventFox = m_Dust = m_Motes = {};
		delete m_DustEffect;
		delete m_MoteEffect;
		m_DustEffect = m_MoteEffect = nullptr;
		delete m_Fox;
		m_Fox = nullptr;
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
		DestroyAndDelete(m_FlipbookTexture);
		DestroyAndDelete(m_UprightTexture);
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
		ImGui::SameLine();
		ImGui::RadioButton("Events", &mode, static_cast<int>(Mode::Events));
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
