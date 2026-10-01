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

	private:
		struct NameHash
		{
			using is_transparent = void;
			size_t operator()(std::string_view name) const { return std::hash<std::string_view>{}(name); }
		};

		std::vector<Joint> m_Joints;
		std::unordered_map<std::string, int32_t, NameHash, std::equal_to<>> m_JointIndices;
		glm::mat4 m_RootTransform{ 1.0f };
		uint32_t  m_SkinJointCount = 0;
	};

}
