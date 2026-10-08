#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <memory>
#include <string>
#include <vector>

namespace Dingo
{

	// GPU particles (ParticleEffect, Renderer3D::CreateParticleEmitter, ParticleEmitterComponent). Start a
	// mode with --particles=fountain (sparks from a cone, through bloom), burst (a burst of sparks at a
	// random point every second), soft (smoke puffs that meet the floor, through the post chain's soft
	// edges) or budget (an emitter keeping about 30,000 particles alive). The panel switches the post
	// chain and soft edges.
	//
	// Over its first five frames it checks, on a private Renderer3D, by reading the particle pool back:
	// an emitter takes the whole 65,536-particle pool and keeps every particle alive, and another finds
	// no room; a burst of 100 gives 100 live particles, and none outlives its lifetime; a ring of 64
	// asked for 100 keeps 64 and counts 36 dropped; a ParticleEmitterComponent's burst through the
	// SceneRenderer lands in its emitter; and a soft particle fades where it meets the floor while its
	// top stays as a hard one draws it.
	class ParticleTest : public GraphicsTest
	{
	public:
		ParticleTest() = default;
		virtual ~ParticleTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		enum class Mode : int
		{
			Fountain,
			Burst,
			Soft,
			Budget
		};

		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }

		void RunCheckStep();
		void CountAlive(Renderer3D& renderer, const ParticleEmitter& emitter, std::function<void(uint32_t alive, float worstAgeOverLife)> done);
		void DrawCheckScene(Framebuffer* target, std::initializer_list<std::pair<ParticleEmitter*, float>> emitters, bool post, const glm::mat4& emitterTransform = glm::mat4(1.0f));
		void DrawLive(float deltaTime);

	private:
		TestChecks m_Checks;
		std::shared_ptr<int> m_Alive;
		int m_CheckStep = 0;

		Mode m_Mode = Mode::Fountain;
		bool m_PostChain = true;
		bool m_SoftEdges = true;
		float m_Time = 0.0f;
		float m_NextBurst = 0.0f;
		PerspectiveCamera m_Camera;

		ParticleEffect* m_FountainEffect = nullptr;
		ParticleEffect* m_BurstEffect = nullptr;
		ParticleEffect* m_SmokeEffect = nullptr;
		ParticleEffect* m_BudgetEffect = nullptr;
		std::shared_ptr<ParticleEmitter> m_Fountain;
		std::shared_ptr<ParticleEmitter> m_Burst;
		std::shared_ptr<ParticleEmitter> m_Smoke;
		std::shared_ptr<ParticleEmitter> m_BudgetEmitter;

		// The checks.
		Renderer3D* m_CheckRenderer = nullptr;
		Framebuffer* m_CheckTarget = nullptr;
		Framebuffer* m_SoftTarget = nullptr;
		Framebuffer* m_HardTarget = nullptr;
		std::vector<std::unique_ptr<ParticleEffect>> m_CheckEffects;
		std::shared_ptr<ParticleEmitter> m_Whole;
		std::shared_ptr<ParticleEmitter> m_NoRoom;
		std::shared_ptr<ParticleEmitter> m_Short;
		std::shared_ptr<ParticleEmitter> m_Small;
		std::shared_ptr<ParticleEmitter> m_SoftPuff;
		std::shared_ptr<ParticleEmitter> m_HardPuff;
		Scene* m_Scene = nullptr;
		Entity m_SceneEmitter;
		std::vector<uint8_t> m_HardPixels;
	};

}
