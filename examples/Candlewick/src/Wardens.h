#pragma once
#include "KeepMap.h"

#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace Dingo
{

	class GameAudio;

	// The keep's guards. Each is a body, a lamp and an eye (a spot light) kept in step by game code,
	// with no collider, so rays never start inside one and one never shoves the player.
	class Wardens
	{
	public:
		enum class State { Patrol, Investigate, Return };

		Wardens(Scene& scene, const KeepMap& map, GameAudio& audio, bool frozen, bool rangeClamp);
		~Wardens();

		Wardens(const Wardens&) = delete;
		Wardens& operator=(const Wardens&) = delete;

		// Walks, aims the eye and clamps its range to the wall it faces.
		void Update(float deltaTime);

		// Every warden back at its route's start, calm.
		void Reset();

		size_t GetCount() const { return m_Wardens.size(); }
		Entity GetEye(size_t index) const { return m_Wardens[index].Eye; }
		Entity GetLamp(size_t index) const { return m_Wardens[index].Lamp; }
		glm::vec3 GetFeet(size_t index) const { return m_Wardens[index].Feet; }
		glm::vec3 GetForward(size_t index) const;
		int GetRoom(size_t index) const { return m_Wardens[index].Room; }
		float GetSuspicion(size_t index) const { return m_Wardens[index].Suspicion; }

		// Detection's verdict for this frame. A last-seen tile sends the warden to look there.
		void SetSuspicion(size_t index, float suspicion, const std::optional<glm::ivec2>& lastSeen);

	private:
		struct LoopPoint
		{
			glm::vec3 Position{ 0.0f };
			bool Waypoint = false;
		};

		struct Part
		{
			Entity Visual;
			glm::vec3 Offset{ 0.0f };
		};

		struct Warden
		{
			size_t Index = 0;
			int Room = -1;
			std::vector<LoopPoint> Loop;
			std::vector<glm::ivec2> LoopTiles;
			std::vector<size_t> LoopTileNext;

			glm::vec3 Feet{ 0.0f };
			float Yaw = 0.0f;
			State Mode = State::Patrol;
			size_t Next = 0;
			float Pause = 0.0f;

			std::vector<glm::vec3> Path;
			size_t PathNext = 0;
			glm::ivec2 Target{ 0 };
			glm::ivec2 Goal{ 0 };
			size_t ReturnTile = 0;
			bool Looking = false;
			float LookTime = 0.0f;
			float LookYaw = 0.0f;

			float Suspicion = 0.0f;
			std::optional<glm::ivec2> Request;
			int MarkerLevel = -1;
			float StepDistance = 0.0f;

			std::vector<Part> Parts;
			Entity Lamp;
			Entity Eye;
			Entity Marker;
		};

		void BuildLoop(Warden& warden, const WardenRoute& route) const;
		void Spawn(Warden& warden, size_t index);
		void AddPart(Warden& warden, const char* name, Mesh* mesh, const glm::vec3& offset, const glm::vec3& size, const glm::vec4& color, Material* material);

		void Think(Warden& warden, size_t index, float deltaTime);
		void Patrol(Warden& warden, float deltaTime);
		void BeginInvestigate(Warden& warden, size_t index, const glm::ivec2& tile);
		void BeginReturn(Warden& warden, size_t index);
		bool FollowPath(Warden& warden, float speed, float deltaTime);
		// Spends part of `timeLeft` getting to `target` and leaves what it did not need.
		bool StepTowards(Warden& warden, const glm::vec3& target, float speed, float& timeLeft);
		bool IsBlockedAhead(const Warden& warden, const glm::vec3& direction) const;
		void SetState(Warden& warden, size_t index, State state);

		std::vector<glm::vec3> SmoothPath(const glm::vec3& start, const std::vector<glm::ivec2>& tiles, int room) const;
		bool IsClearLine(const glm::vec3& from, const glm::vec3& to, int room) const;

		void Place(Warden& warden);
		void ClampRange(Warden& warden, float deltaTime, bool snap);
		void UpdateMarker(Warden& warden);

	private:
		Scene& m_Scene;
		const KeepMap& m_Map;
		GameAudio& m_Audio;
		bool m_Frozen = false;
		bool m_RangeClamp = true;

		Material* m_ArmourMaterial = nullptr;
		Material* m_VisorMaterial = nullptr;
		Material* m_LampMaterial = nullptr;
		Material* m_MarkerMaterials[3] = {};

		std::vector<Warden> m_Wardens;
	};

}
