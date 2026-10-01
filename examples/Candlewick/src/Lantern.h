#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstddef>
#include <vector>

namespace Dingo
{

	class GameAudio;
	class Player;

	// The player's light: its radius is the oil it has left. Update runs after Player, so the
	// strike lock it sets takes effect on the player's next frame.
	class Lantern
	{
	public:
		enum class State { Lit, Snuffed, Striking };

		Lantern(Scene& scene, const Player& player, Material* frameMaterial, GameAudio& audio, float startOil, bool burns);
		~Lantern();

		Lantern(const Lantern&) = delete;
		Lantern& operator=(const Lantern&) = delete;

		void Update(float deltaTime, Player& player);
		void AddOil(float amount);
		void SetOil(float oil);

		Entity GetLight() const { return m_Light; }
		State GetState() const { return m_State; }
		float GetOil() const { return m_Oil; }

		size_t GetFlaskCapacity() const;

		// A strike must leave some flame behind, so it needs more oil than it costs.
		bool CanRelight() const;
		bool IsOutOfOil() const { return m_State == State::Snuffed && m_Oil <= 0.0f; }
		bool IsTooLowToStrike() const { return m_State == State::Snuffed && m_Oil > 0.0f && !CanRelight(); }

	private:
		struct Part
		{
			Entity Visual;
			glm::vec3 Offset{ 0.0f };
		};

		void AddPart(Scene& scene, const char* name, const glm::vec3& offset, const glm::vec3& size, const glm::vec4& color, Material* material);
		glm::vec3 FindHand(const glm::vec3& axis, const glm::quat& facing) const;
		void Place(const Player& player, float deltaTime);
		void Apply();

	private:
		Scene& m_Scene;
		GameAudio& m_Audio;
		Entity m_Light;
		std::vector<Part> m_Parts;

		Material* m_GlassMaterial = nullptr;

		State m_State = State::Lit;
		float m_Oil = 0.0f;
		bool m_Burns = true;
		bool m_SnuffQueued = false;
		float m_StrikeTime = 0.0f;
		float m_Clock = 0.0f;
		glm::vec3 m_Hand{ 0.0f };
	};

}
