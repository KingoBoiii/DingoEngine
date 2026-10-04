#include "Checks.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "Moveset.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

namespace
{
	using namespace Dingo;

	class Report
	{
	public:
		void Check(bool passed, const std::string& name)
		{
			if (passed)
			{
				++m_Passed;
				DE_INFO("[PASS] {}", name);
			}
			else
			{
				++m_Failed;
				DE_ERROR("[FAIL] {}", name);
			}
		}

		int GetPassed() const { return m_Passed; }
		int GetFailed() const { return m_Failed; }

	private:
		int m_Passed = 0;
		int m_Failed = 0;
	};

	struct PoseDiff
	{
		float Rotation = 0.0f;
		float Translation = 0.0f;
		uint32_t Matched = 0;
		uint32_t Missing = 0;
		std::vector<std::string> Extra;
	};

	PoseDiff ComparePoses(const Skeleton& target, std::span<const JointPose> targetPoses, const Skeleton& reference, std::span<const JointPose> referencePoses)
	{
		PoseDiff diff;
		for (uint32_t i = 0; i < target.GetJointCount(); ++i)
		{
			const int32_t other = reference.FindJoint(target.GetJoint(i).Name);
			if (other == Skeleton::k_InvalidJoint)
			{
				diff.Extra.push_back(target.GetJoint(i).Name);
				continue;
			}
			++diff.Matched;

			const JointPose& a = targetPoses[i];
			const JointPose& b = referencePoses[static_cast<size_t>(other)];

			const glm::quat signedB = glm::dot(a.Rotation, b.Rotation) < 0.0f ? -b.Rotation : b.Rotation;
			diff.Rotation = std::max({ diff.Rotation, std::abs(a.Rotation.x - signedB.x), std::abs(a.Rotation.y - signedB.y),
				std::abs(a.Rotation.z - signedB.z), std::abs(a.Rotation.w - signedB.w) });

			const glm::vec3 gap = glm::abs(a.Translation - b.Translation);
			diff.Translation = std::max({ diff.Translation, gap.x, gap.y, gap.z });
		}
		diff.Missing = reference.GetJointCount() - diff.Matched;
		return diff;
	}

	std::string Join(const std::vector<std::string>& names)
	{
		std::string joined;
		for (const std::string& name : names)
		{
			if (!joined.empty())
				joined += ", ";
			joined += name;
		}
		return joined;
	}

	void CheckAssets(Report& report, const GameAssets& assets)
	{
		std::vector<std::string> failed;
		for (const FighterDef& fighter : GetFighterDefs())
		{
			const Model* model = assets.GetCharacter(fighter);
			if (!model || !model->IsSkinned())
				failed.emplace_back(fighter.Model);
		}
		report.Check(failed.empty(), std::format("all {} character models load with a skeleton{}{}", GetFighterDefs().size(),
			failed.empty() ? "" : "; not: ", Join(failed)));

		failed.clear();
		const std::span<const LibraryDef> libraryDefs = GetLibraryDefs();
		for (size_t i = 0; i < libraryDefs.size(); ++i)
		{
			const Model* library = assets.GetLibraries()[i];
			if (!library || !library->IsSkinned() || library->GetAnimationCount() == 0)
				failed.emplace_back(libraryDefs[i].Path);
		}
		report.Check(failed.empty(), std::format("all {} clip libraries load with a skeleton and clips{}{}", libraryDefs.size(),
			failed.empty() ? "" : "; not: ", Join(failed)));

		failed.clear();
		size_t weapons = 0;
		for (const FighterDef& fighter : GetFighterDefs())
		{
			for (const char* path : { fighter.RightWeapon, fighter.LeftWeapon })
			{
				if (!path)
					continue;
				++weapons;
				const Model* weapon = assets.GetModel(path);
				if (!weapon || weapon->GetSubMeshCount() == 0)
					failed.emplace_back(path);
			}
		}
		report.Check(failed.empty(), std::format("all {} weapon models load with meshes{}{}", weapons, failed.empty() ? "" : "; not: ", Join(failed)));

		failed.clear();
		for (const FighterDef& fighter : GetFighterDefs())
		{
			if (!assets.GetTexture(fighter.Texture) || !assets.GetMaterial(fighter))
				failed.emplace_back(fighter.Texture);
		}
		report.Check(failed.empty(), std::format("every fighter has its texture on a lit material{}{}", failed.empty() ? "" : "; not: ", Join(failed)));
	}

	void CheckClips(Report& report, const GameAssets& assets)
	{
		const ClipSet& clips = assets.GetClips();
		report.Check(clips.GetMissing().empty() && !clips.GetEntries().empty(),
			std::format("every clip the Moveset names is found in the libraries ({} clips{}{})", clips.GetEntries().size(),
				clips.GetMissing().empty() ? "" : "; missing: ", Join(clips.GetMissing())));
	}

	void CheckFighter(Report& report, const GameAssets& assets, const FighterDef& fighter)
	{
		const Model* model = assets.GetCharacter(fighter);
		const Skeleton* skeleton = model ? model->GetSkeleton() : nullptr;
		if (!skeleton)
		{
			report.Check(false, std::format("{}: has a skeleton", fighter.Name));
			return;
		}

		const int32_t right = skeleton->FindJoint(Joints::HAND_RIGHT);
		const int32_t left = skeleton->FindJoint(Joints::HAND_LEFT);
		const bool needsLeft = fighter.LeftWeapon != nullptr;
		report.Check(right != Skeleton::k_InvalidJoint && (!needsLeft || left != Skeleton::k_InvalidJoint),
			std::format("{}: {} resolves to joint {}{}", fighter.Name, Joints::HAND_RIGHT, right,
				needsLeft ? std::format(", {} to joint {}", Joints::HAND_LEFT, left) : std::string()));

		uint32_t widest = 0;
		uint32_t skinned = 0;
		for (const SubMesh& submesh : model->GetSubMeshes())
		{
			if (submesh.MeshData && submesh.MeshData->HasSkin())
			{
				++skinned;
				widest = std::max(widest, submesh.MeshData->GetSkinJointCount());
			}
		}
		report.Check(skeleton->GetSkinJointCount() <= Renderer3D::k_MaxSkinJoints && widest <= Renderer3D::k_MaxSkinJoints && skinned > 0,
			std::format("{}: {} joints, {} skin joints, {} skinned submeshes (widest {}), within the {} a draw can skin", fighter.Name,
				skeleton->GetJointCount(), skeleton->GetSkinJointCount(), skinned, widest, Renderer3D::k_MaxSkinJoints));

		const AnimationClip* idle = assets.GetClip(Clips::IDLE);
		const Skeleton* librarySkeleton = idle ? idle->GetSourceSkeleton() : nullptr;
		if (!idle || !librarySkeleton)
		{
			report.Check(false, std::format("{}: {} is available to compare poses", fighter.Name, Clips::IDLE));
			return;
		}

		Animator retargeted(skeleton);
		Animator native(librarySkeleton);
		for (Animator* animator : { &retargeted, &native })
		{
			animator->Play(idle);
			animator->SetTime(CHECK_POSE_TIME);
			animator->Evaluate();
		}

		std::vector<JointPose> restPoses;
		restPoses.reserve(librarySkeleton->GetJointCount());
		for (uint32_t i = 0; i < librarySkeleton->GetJointCount(); ++i)
			restPoses.push_back(librarySkeleton->GetJoint(i).RestPose);
		const PoseDiff fromRest = ComparePoses(*librarySkeleton, native.GetLocalPoses(), *librarySkeleton, restPoses);
		const float deviation = std::max(fromRest.Rotation, fromRest.Translation);

		const PoseDiff diff = ComparePoses(*skeleton, retargeted.GetLocalPoses(), *librarySkeleton, native.GetLocalPoses());
		report.Check(librarySkeleton != skeleton && deviation > CHECK_POSE_TOLERANCE && diff.Missing == 0
				&& diff.Rotation <= CHECK_POSE_TOLERANCE && diff.Translation <= CHECK_POSE_TOLERANCE,
			std::format("{}: {} at {:.1f} s retargeted from the library matches the library's own pose on all {} of its joints "
				"(worst rotation {:.2e}, translation {:.2e}, tolerance {:.0e}; the pose leaves the rest pose by {:.2e}{}{}{})", fighter.Name, Clips::IDLE, CHECK_POSE_TIME,
				diff.Matched, diff.Rotation, diff.Translation, CHECK_POSE_TOLERANCE, deviation,
				diff.Missing ? std::format("; {} missing", diff.Missing) : std::string(),
				diff.Extra.empty() ? "" : "; the fighter's own: ", Join(diff.Extra)));
	}

	void LogHipsTravel(const GameAssets& assets)
	{
		const Model* general = assets.GetLibraries().empty() ? nullptr : assets.GetLibraries()[0];
		if (general && general->GetSkeleton())
		{
			const Skeleton& skeleton = *general->GetSkeleton();
			const int32_t hips = skeleton.FindJoint(Joints::HIPS);
			if (hips != Skeleton::k_InvalidJoint)
			{
				const glm::vec3 rest = skeleton.GetJoint(static_cast<uint32_t>(hips)).RestPose.Translation;
				DE_INFO("[INFO] hips translation per clip the Moveset uses, in metres in the root's frame; rest ({:.3f}, {:.3f}, {:.3f})", rest.x, rest.y, rest.z);
			}
		}

		const std::span<const LibraryDef> libraryDefs = GetLibraryDefs();
		for (const ClipSet::Entry& entry : assets.GetClips().GetEntries())
		{
			const char* library = entry.Library < libraryDefs.size() ? libraryDefs[entry.Library].Label : "?";
			const AnimationChannel* hips = entry.Clip->FindChannel(Joints::HIPS);
			if (!hips || hips->Translation.IsEmpty())
			{
				DE_INFO("[INFO]   {:<34} {:<17} {:>5.2f} s   no hips translation keys", entry.Name, library, entry.Clip->GetDuration());
				continue;
			}

			glm::vec3 low(hips->Translation.Values.front());
			glm::vec3 high(low);
			for (const glm::vec3& value : hips->Translation.Values)
			{
				low = glm::min(low, value);
				high = glm::max(high, value);
			}

			const float span = std::max(high.x - low.x, high.z - low.z);
			DE_INFO("[INFO]   {:<34} {:<17} {:>5.2f} s   x [{:>7.3f}, {:>7.3f}]  y [{:>7.3f}, {:>7.3f}]  z [{:>7.3f}, {:>7.3f}]  ground span {:.3f}{}",
				entry.Name, library, entry.Clip->GetDuration(), low.x, high.x, low.y, high.y, low.z, high.z, span,
				span > CHECK_HIPS_TRAVEL ? "  travels" : "");
		}
	}
}

namespace Dingo
{

	bool RunAssetChecks(const GameAssets& assets)
	{
		Report report;
		CheckAssets(report, assets);
		CheckClips(report, assets);
		for (const FighterDef& fighter : GetFighterDefs())
			CheckFighter(report, assets, fighter);
		LogHipsTravel(assets);

		if (report.GetFailed() == 0)
			DE_INFO("Asset checks: {} passed, {} failed", report.GetPassed(), report.GetFailed());
		else
			DE_ERROR("Asset checks: {} passed, {} failed", report.GetPassed(), report.GetFailed());
		return report.GetFailed() == 0;
	}

}
