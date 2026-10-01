#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace Dingo
{

	class KeepWorld;
	class Lantern;
	class Player;
	class Wardens;

	// Turns what each warden sees into its suspicion. The cone test is the renderer's own weight for
	// the warden's eye light (GetLightAttenuation on the same component the frame draws), so the
	// pool on the floor is where a warden sees you; a ray then keeps walls honest. Only the cone can
	// take suspicion to 1: the beacon (a lit player is noticed from further off) stops at alert, and
	// touching a warden (within TOUCH_DISTANCE, since wardens have no collider) holds it at 0.6 or more.
	class Detection
	{
	public:
		static constexpr size_t k_SampleCount = 3;

		Detection(Scene& scene, const KeepWorld& world, size_t wardenCount, bool debugView, bool canCatch);
		~Detection();

		Detection(const Detection&) = delete;
		Detection& operator=(const Detection&) = delete;

		// Returns the warden that caught the player this frame, if one did.
		std::optional<size_t> Update(float deltaTime, Wardens& wardens, const Player& player, const Lantern& lantern);

		// Redraws the --debug-cone view on its own, for frames that skip Update.
		void UpdateDebugView(const Wardens& wardens, const Player& player);

	private:
		struct Sconce
		{
			Entity Light;
			float BaseIntensity = 0.0f;
		};

		struct EyeState
		{
			glm::vec3 Position{ 0.0f };
			glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
			float Range = -1.0f;
			bool Enabled = false;
			bool HasPhysics = false;

			bool operator==(const EyeState& other) const
			{
				return Position == other.Position && Rotation == other.Rotation && Range == other.Range
					&& Enabled == other.Enabled && HasPhysics == other.HasPhysics;
			}
		};

		bool HasLineOfSight(const glm::vec3& eye, const glm::vec3& target) const;
		bool IsFlameLit(const glm::vec3& point) const;

		void CreateDebugView(size_t wardenCount);
		Mesh* BuildFootprint(const SpotLight& eye);

	private:
		Scene& m_Scene;
		std::vector<Sconce> m_Sconces;
		std::vector<Entity> m_Braziers;
		bool m_CanCatch = true;
		bool m_DebugView = false;
		bool m_SampleSeen[k_SampleCount] = {};

		Mesh* m_ConeMesh = nullptr;
		Material* m_ConeMaterial = nullptr;
		Material* m_DotMaterial = nullptr;
		Material* m_SeenMaterial = nullptr;
		Material* m_UnseenMaterial = nullptr;
		std::vector<Entity> m_Cones;
		std::vector<Entity> m_Footprints;
		std::vector<Mesh*> m_FootprintMeshes;
		std::vector<EyeState> m_FootprintEyes;
		std::vector<Entity> m_Samples;
		std::vector<MeshVertex> m_DotVertices;
		std::vector<uint32_t> m_DotIndices;
	};

}
