#include "depch.h"
#include "DingoEngine/Graphics/Skeleton.h"
#include "DingoEngine/Log.h"

#include <glm/gtc/matrix_transform.hpp>

#include <atomic>

namespace Dingo
{

	uint64_t Skeleton::AllocateId()
	{
		static std::atomic<uint64_t> s_NextId{ 1 };
		return s_NextId.fetch_add(1, std::memory_order_relaxed);
	}

	glm::mat4 JointPose::ToMatrix() const
	{
		// translate x rotate x scale without the two 4x4 products: the same values, since every term
		// those products add is a zero or a multiply by one.
		glm::mat4 matrix = glm::mat4_cast(Rotation);
		matrix[0] *= Scale.x;
		matrix[1] *= Scale.y;
		matrix[2] *= Scale.z;
		matrix[3] = glm::vec4(Translation, 1.0f);
		return matrix;
	}

	Skeleton::Skeleton(std::vector<Joint> joints, const glm::mat4& rootTransform, uint32_t skinJointCount)
		: m_Joints(std::move(joints)), m_RootTransform(rootTransform), m_SkinJointCount(skinJointCount)
	{
		DE_CORE_ASSERT(skinJointCount <= m_Joints.size(), "Skeleton skin joint count exceeds its joint count");
		m_JointIndices.reserve(m_Joints.size());
		for (int32_t i = 0; i < static_cast<int32_t>(m_Joints.size()); ++i)
		{
			DE_CORE_ASSERT(m_Joints[i].Parent < i, "Skeleton joints must be ordered parents first");
			m_JointIndices.try_emplace(m_Joints[i].Name, i);
		}

		std::vector<JointPose> restPoses;
		restPoses.reserve(m_Joints.size());
		for (const Joint& joint : m_Joints)
			restPoses.push_back(joint.RestPose);

		m_RestGlobals.resize(m_Joints.size());
		ComputeGlobalTransforms(restPoses, m_RestGlobals);

		m_RestPalette.resize(m_SkinJointCount);
		ComputeSkinningPalette(m_RestGlobals, m_RestPalette);
	}

	void Skeleton::Reinitialize(Skeleton& source)
	{
		m_Joints = std::move(source.m_Joints);
		m_JointIndices = std::move(source.m_JointIndices);
		m_RootTransform = source.m_RootTransform;
		m_SkinJointCount = source.m_SkinJointCount;
		m_RestGlobals = std::move(source.m_RestGlobals);
		m_RestPalette = std::move(source.m_RestPalette);
		++m_Revision;
	}

	int32_t Skeleton::FindJoint(std::string_view name) const
	{
		auto it = m_JointIndices.find(name);
		return it != m_JointIndices.end() ? it->second : k_InvalidJoint;
	}

	void Skeleton::ComputeGlobalTransforms(std::span<const JointPose> localPoses, std::span<glm::mat4> outGlobals) const
	{
		DE_CORE_ASSERT(localPoses.size() >= m_Joints.size() && outGlobals.size() >= m_Joints.size(), "Skeleton::ComputeGlobalTransforms needs one pose and one output per joint");

		for (size_t i = 0; i < m_Joints.size(); ++i)
		{
			const glm::mat4 local = localPoses[i].ToMatrix();
			const int32_t parent = m_Joints[i].Parent;
			outGlobals[i] = parent < 0 ? local : outGlobals[parent] * local;
		}
	}

	void Skeleton::ComputeSkinningPalette(std::span<const glm::mat4> globals, std::span<glm::mat4> outPalette) const
	{
		DE_CORE_ASSERT(globals.size() >= m_SkinJointCount && outPalette.size() >= m_SkinJointCount, "Skeleton::ComputeSkinningPalette needs a global and an output per skin joint");

		// Most files have no transform above the skeleton; multiplying by identity changes nothing.
		if (m_RootTransform == glm::mat4(1.0f))
		{
			for (uint32_t i = 0; i < m_SkinJointCount; ++i)
				outPalette[i] = globals[i] * m_Joints[i].InverseBind;
			return;
		}

		for (uint32_t i = 0; i < m_SkinJointCount; ++i)
			outPalette[i] = m_RootTransform * globals[i] * m_Joints[i].InverseBind;
	}

}
