#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	struct JointPose
	{
		glm::vec3 Translation{ 0.0f };
		glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale{ 1.0f };

		glm::mat4 ToMatrix() const;
	};

	struct Joint
	{
		std::string Name;
		int32_t     Parent = -1;
		// Maps a skin vertex into this joint's space at bind time.
		glm::mat4   InverseBind{ 1.0f };
		// The joint's local transform in the file's rest pose. A skin's bind pose is what
		// InverseBind undoes; in glTF the two may differ.
		JointPose   RestPose;
	};

	class Skeleton
	{
	public:
		static constexpr int32_t k_InvalidJoint = -1;

	public:
		// Joints must be ordered parents first: every Parent is -1 or a smaller index.
		Skeleton(std::vector<Joint> joints, const glm::mat4& rootTransform, uint32_t skinJointCount);
		// Copies would share an id.
		Skeleton(const Skeleton&) = delete;
		Skeleton& operator=(const Skeleton&) = delete;

		// Never reused, so an animator bound to a freed skeleton can tell that a new one at the same
		// address is not the same.
		uint64_t GetId() const { return m_Id; }
		// Bumped by every model reload that keeps the joints (names and parents), which may bring new
		// rest poses and inverse binds; the id stays, so animators keep playing and pick them up at
		// their next update.
		uint32_t GetRevision() const { return m_Revision; }

		uint32_t                  GetJointCount() const { return static_cast<uint32_t>(m_Joints.size()); }
		const std::vector<Joint>& GetJoints()     const { return m_Joints; }
		const Joint&              GetJoint(uint32_t index) const { return m_Joints[index]; }

		// Skin vertices only index joints below this, so a skinning palette needs only these.
		// Helper and end joints that no vertex follows come after them.
		uint32_t GetSkinJointCount() const { return m_SkinJointCount; }

		// k_InvalidJoint when no joint has that name. With duplicate names the first wins.
		int32_t FindJoint(std::string_view name) const;

		// The file's transform above the skeleton's root joint. Global joint transforms are
		// relative to it, so model space = RootTransform * global.
		const glm::mat4& GetRootTransform() const { return m_RootTransform; }

		// Composes local poses (one per joint) into joint transforms relative to the root
		// transform, in one forward pass.
		void ComputeGlobalTransforms(std::span<const JointPose> localPoses, std::span<glm::mat4> outGlobals) const;

		// What a skinned draw uploads, one matrix per skin joint: RootTransform * global *
		// InverseBind, taking a skin vertex to model space.
		void ComputeSkinningPalette(std::span<const glm::mat4> globals, std::span<glm::mat4> outPalette) const;

		// The palette of the rest pose, for drawing a skinned mesh nothing animates.
		const std::vector<glm::mat4>& GetRestPalette() const { return m_RestPalette; }
		// ComputeGlobalTransforms of every joint's RestPose.
		const std::vector<glm::mat4>& GetRestGlobalTransforms() const { return m_RestGlobals; }

	private:
		static uint64_t AllocateId();

		// Takes the source's joints, which must match these by name and parent.
		void Reinitialize(Skeleton& source);

		friend class Model;

		struct NameHash
		{
			using is_transparent = void;
			size_t operator()(std::string_view name) const { return std::hash<std::string_view>{}(name); }
		};

		uint64_t m_Id = AllocateId();
		uint32_t m_Revision = 0;
		std::vector<Joint> m_Joints;
		std::unordered_map<std::string, int32_t, NameHash, std::equal_to<>> m_JointIndices;
		glm::mat4 m_RootTransform{ 1.0f };
		uint32_t  m_SkinJointCount = 0;
		std::vector<glm::mat4> m_RestGlobals;
		std::vector<glm::mat4> m_RestPalette;
	};

}
