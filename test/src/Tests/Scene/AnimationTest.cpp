#include "AnimationTest.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <limits>
#include <optional>

namespace
{
	using namespace Dingo;

	using Clock = std::chrono::steady_clock;

	constexpr const char* k_FoxPath = "assets/models/Fox/Fox.gltf";
	constexpr const char* k_HatJoint = "b_Head_05";
	constexpr const char* k_UpperBody = "b_Spine01_02";
	constexpr const char* k_SpeedParameter = "Speed";
	constexpr float k_FoxLength = 1.6f;
	constexpr float k_CrowdSpacing = 2.2f;
	constexpr float k_WarmupSeconds = 2.0f;
	constexpr uint32_t k_MeasuredFrames = 120;
	// In the Fox's model units (it is about 150 long), in the head joint's frame, whose x runs along
	// the snout and y up through the crown: a box sitting between the ears.
	constexpr float k_HatSize = 10.0f;
	const glm::vec3 k_HatOffset{ 2.0f, 20.0f, 0.0f };

	double Milliseconds(Clock::time_point from, Clock::time_point to)
	{
		return std::chrono::duration<double, std::milli>(to - from).count();
	}

	const SubMesh* FindSkinnedSubMesh(const Model& model)
	{
		for (const SubMesh& submesh : model.GetSubMeshes())
		{
			if (submesh.MeshData->HasSkin())
				return &submesh;
		}
		return nullptr;
	}

	glm::quat Turn(float degrees, const glm::vec3& axis)
	{
		return glm::angleAxis(glm::radians(degrees), axis);
	}

	// Sign-blind: q and -q are the same rotation.
	float QuatGap(const glm::quat& a, const glm::quat& b)
	{
		const glm::vec4 va(a.x, a.y, a.z, a.w);
		const glm::vec4 vb(b.x, b.y, b.z, b.w);
		return (std::min)(glm::length(va - vb), glm::length(va + vb));
	}

	float PoseGap(std::span<const JointPose> a, std::span<const JointPose> b)
	{
		float worst = 0.0f;
		for (size_t i = 0; i < a.size(); ++i)
			worst = (std::max)({ worst, glm::length(a[i].Translation - b[i].Translation), QuatGap(a[i].Rotation, b[i].Rotation), glm::length(a[i].Scale - b[i].Scale) });
		return worst;
	}

	std::vector<JointPose> PoseAt(const Skeleton& skeleton, const AnimationClip* clip, float time, bool loop = true)
	{
		Animator animator(&skeleton);
		animator.Play(AnimationState::Clip(clip).SetLoop(loop));
		animator.SetTime(time);
		animator.Update(0.0f);
		return { animator.GetLocalPoses().begin(), animator.GetLocalPoses().end() };
	}

	// The cross-fade the Animator applies: lerp, and a normalised lerp along the shorter arc.
	void Blend(std::vector<JointPose>& base, const std::vector<JointPose>& top, float weight)
	{
		for (size_t i = 0; i < base.size(); ++i)
		{
			base[i].Translation = glm::mix(base[i].Translation, top[i].Translation, weight);
			base[i].Scale = glm::mix(base[i].Scale, top[i].Scale, weight);
			const glm::quat to = glm::dot(base[i].Rotation, top[i].Rotation) < 0.0f ? -top[i].Rotation : top[i].Rotation;
			base[i].Rotation = glm::normalize(base[i].Rotation * (1.0f - weight) + to * weight);
		}
	}

	// A joint transform without its scale: what a child on that joint hangs from.
	glm::mat4 SocketFrame(const glm::mat4& joint)
	{
		glm::mat4 frame(1.0f);
		frame[0] = glm::vec4(glm::normalize(glm::vec3(joint[0])), 0.0f);
		frame[1] = glm::vec4(glm::normalize(glm::vec3(joint[1])), 0.0f);
		frame[2] = glm::vec4(glm::normalize(glm::vec3(joint[2])), 0.0f);
		frame[3] = joint[3];
		return frame;
	}
}

namespace Dingo
{

	void AnimationTest::Check(bool condition, const std::string& name)
	{
		m_Checks.push_back({ name, condition });
		if (condition)
			DE_INFO("[PASS] {}", name);
		else
			DE_ERROR("[FAIL] {}", name);
	}

	void AnimationTest::Initialize()
	{
		m_Checks.clear();
		m_DrawChecksDone = false;
		m_Time = 0.0f;
		m_TimedFrames = 0;
		m_FrameMs = m_UpdateMs = m_RenderMs = m_EndSceneMs = 0.0;
		m_TimingResult.clear();

		if (!m_ArgsRead)
		{
			m_ArgsRead = true;
			const ApplicationCommandLineArgs& args = Application::Get().GetCommandLineArgs();
			if (auto mode = args.Get("anim"))
			{
				m_Mode = *mode == "bindstatic" ? Mode::BindStatic : *mode == "pose" ? Mode::Pose
					: *mode == "clip" ? Mode::Clip : *mode == "blend" ? Mode::Blend : *mode == "layers" ? Mode::Layers
					: *mode == "crowd" ? Mode::Crowd : Mode::Bind;
				if (m_Mode == Mode::Bind && *mode != "bind")
					DE_WARN("Animation Test: unknown --anim={}; showing bind. Use bind, bindstatic, pose, clip, blend, layers or crowd.", *mode);
			}
			if (auto count = args.Get("anim-count"); count && !count->empty())
				m_CrowdCount = static_cast<uint32_t>((std::max)(1l, std::strtol(std::string(*count).c_str(), nullptr, 10)));
			m_CrowdStatic = args.Get("anim-static").has_value();
			if (auto clip = args.Get("anim-clip"); clip && !clip->empty())
				m_ClipName = std::string(*clip);
			if (auto time = args.Get("anim-time"); time && !time->empty())
				m_FreezeTime = (std::max)(0.0f, std::strtof(std::string(*time).c_str(), nullptr));
			if (auto phase = args.Get("anim-phase"); phase && !phase->empty())
				m_FreezePhase = std::clamp(std::strtof(std::string(*phase).c_str(), nullptr), 0.0f, 1.0f);
			if (auto speed = args.Get("anim-speed"); speed && !speed->empty())
				m_BlendSpeed = std::strtof(std::string(*speed).c_str(), nullptr);
		}

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.05f, 200.0f);

		m_Fox = Model::LoadFromFile(k_FoxPath);
		RunLoadChecks();
		if (!m_Fox || !FindSkinnedSubMesh(*m_Fox))
			return;

		glm::vec3 minBounds((std::numeric_limits<float>::max)());
		glm::vec3 maxBounds((std::numeric_limits<float>::lowest)());
		for (const SubMesh& submesh : m_Fox->GetSubMeshes())
		{
			for (const MeshVertex& vertex : submesh.MeshData->GetVertices())
			{
				minBounds = (glm::min)(minBounds, vertex.Position);
				maxBounds = (glm::max)(maxBounds, vertex.Position);
			}
		}
		const glm::vec3 extent = maxBounds - minBounds;
		m_FoxScale = k_FoxLength / (std::max)({ extent.x, extent.y, extent.z });
		const glm::vec3 center = 0.5f * (minBounds + maxBounds);
		m_FoxOffset = -glm::vec3(center.x, minBounds.y, center.z) * m_FoxScale;
		m_FoxCenter = glm::vec3(0.0f, 0.5f * extent.y * m_FoxScale, 0.0f);
		m_FoxRadius = 0.5f * glm::length(extent) * m_FoxScale;

		RunAnimatorChecks();
		RunBlendChecks();
		RunSceneChecks();

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		m_FoxMaterial = renderer.CreateLitMaterial(MaterialParams().SetDebugName("Fox"));
		if (Texture* diffuse = FindSkinnedSubMesh(*m_Fox)->DiffuseTexture)
			m_FoxMaterial->SetTexture(0, diffuse);

		BuildScene();
	}

	void AnimationTest::RunLoadChecks()
	{
		Check(m_Fox && m_Fox->IsSkinned(), "Fox.gltf loads with a skeleton");
		if (!m_Fox || !m_Fox->IsSkinned())
			return;

		const Skeleton& skeleton = *m_Fox->GetSkeleton();
		Check(skeleton.GetJointCount() == 24 && skeleton.GetSkinJointCount() == 24, "24 joints, all reachable by the skin");
		Check(m_Fox->GetAnimationCount() == 3 && m_Fox->FindAnimation("Survey") && m_Fox->FindAnimation("Walk") && m_Fox->FindAnimation("Run"),
			"3 clips: Survey, Walk, Run");

		float worstPalette = 0.0f;
		for (const glm::mat4& matrix : skeleton.GetRestPalette())
		{
			for (int column = 0; column < 4; ++column)
				for (int row = 0; row < 4; ++row)
					worstPalette = (std::max)(worstPalette, std::abs(matrix[column][row] - (column == row ? 1.0f : 0.0f)));
		}
		Check(skeleton.GetRestPalette().size() == 24 && worstPalette < 1e-3f, std::format("the rest palette is identity (worst {:.1e})", worstPalette));

		const SubMesh* skinned = FindSkinnedSubMesh(*m_Fox);
		Check(skinned && skinned->MeshData->GetSkinJointCount() == 24 && skinned->MeshData->GetSkinJointCount() <= Renderer3D::k_MaxSkinJoints,
			"the skin reads 24 palette entries, within k_MaxSkinJoints");
		if (!skinned)
			return;

		float worstWeights = 0.0f;
		for (const SkinnedMeshVertex& vertex : skinned->MeshData->GetSkinVertices())
			worstWeights = (std::max)(worstWeights, std::abs(vertex.Weights.x + vertex.Weights.y + vertex.Weights.z + vertex.Weights.w - 1.0f));
		Check(worstWeights < 1e-5f, "every skin vertex's weights sum to 1");
	}

	void AnimationTest::RunAnimatorChecks()
	{
		const Skeleton& skeleton = *m_Fox->GetSkeleton();
		const AnimationClip* walk = m_Fox->FindAnimation("Walk");
		const AnimationClip* run = m_Fox->FindAnimation("Run");
		if (!walk || !run)
			return;

		const AnimationChannel* channel = nullptr;
		for (const AnimationChannel& candidate : walk->GetChannels())
		{
			if (candidate.Rotation.Times.size() >= 3 && skeleton.FindJoint(candidate.JointName) != Skeleton::k_InvalidJoint)
			{
				channel = &candidate;
				break;
			}
		}
		if (channel)
		{
			const int32_t joint = skeleton.FindJoint(channel->JointName);
			const std::vector<float>& times = channel->Rotation.Times;
			const std::vector<glm::quat>& values = channel->Rotation.Values;
			const float atKey = QuatGap(PoseAt(skeleton, walk, times[1])[joint].Rotation, values[1]);
			const float between = QuatGap(PoseAt(skeleton, walk, 0.5f * (times[1] + times[2]))[joint].Rotation, glm::slerp(values[1], values[2], 0.5f));
			Check(atKey < 1e-6f && between < 1e-5f, std::format("Walk sampled on a key gives that key, and halfway to the next their slerp ({}: {:.1e}, {:.1e})", channel->JointName, atKey, between));
		}

		const float duration = walk->GetDuration();
		{
			Animator animator(&skeleton);
			animator.Play(walk);
			animator.Update(0.6f * duration);
			animator.Update(0.6f * duration);
			const float gap = PoseGap(animator.GetLocalPoses(), PoseAt(skeleton, walk, 0.2f * duration));
			Check(std::abs(animator.GetTime() - 0.2f * duration) < 1e-4f && gap < 1e-4f, std::format("a looping clip wraps past its end (t = {:.4f} of {:.4f})", animator.GetTime(), duration));
		}
		{
			Animator animator(&skeleton);
			animator.Play(AnimationState::Clip(walk).SetLoop(false));
			animator.Update(duration + 1.0f);
			const float gap = PoseGap(animator.GetLocalPoses(), PoseAt(skeleton, walk, duration, false));
			Check(animator.IsFinished() && animator.GetTime() == duration && gap < 1e-6f, "a clip that doesn't loop stops on its last frame");

			animator.Play(AnimationState::Clip(walk).SetLoop(false));
			animator.Update(0.0f);
			const bool stillFinished = animator.IsFinished() && animator.GetTime() == duration;
			animator.Update(-(duration + 1.0f));
			Check(stillFinished && animator.IsFinished() && animator.GetTime() == 0.0f,
				"playing a finished one-shot again leaves it finished, and running time backwards finishes it at its start");
		}
		{
			Animator animator(&skeleton);
			animator.Play(walk);
			animator.Update(0.1f);
			const std::vector<JointPose> before(animator.GetLocalPoses().begin(), animator.GetLocalPoses().end());

			animator.Play(run, 0.4f);
			animator.Update(0.0f);
			const float start = PoseGap(animator.GetLocalPoses(), before);

			animator.Update(0.2f);
			std::vector<JointPose> expected = PoseAt(skeleton, walk, 0.3f);
			Blend(expected, PoseAt(skeleton, run, 0.2f), 0.5f);
			const float middle = PoseGap(animator.GetLocalPoses(), expected);
			const bool fadingMidway = animator.IsFading();

			animator.Update(0.2f);
			const float end = PoseGap(animator.GetLocalPoses(), PoseAt(skeleton, run, 0.4f));
			Check(start < 1e-6f && middle < 1e-5f && end < 1e-6f && fadingMidway && !animator.IsFading() && animator.GetCurrentClip() == run,
				std::format("a 0.4 s cross-fade starts on Walk, is half Walk, half Run at 0.2 s and ends on Run ({:.1e}, {:.1e}, {:.1e})", start, middle, end));

			animator.Play(run, 0.4f);
			Check(!animator.IsFading() && std::abs(animator.GetTime() - 0.4f) < 1e-6f, "playing the clip that already plays changes nothing");
		}
		{
			Animator animator(&skeleton);
			animator.Play(walk);
			animator.Update(0.1f);
			animator.Play(run, 0.4f);
			animator.Update(0.1f);
			animator.Play(run);
			animator.Update(0.0f);
			const float gap = PoseGap(animator.GetLocalPoses(), PoseAt(skeleton, run, 0.1f));
			Check(!animator.IsFading() && gap < 1e-6f, "a cut to the clip that is fading in finishes its fade");
		}
		{
			std::optional<Animator> original(std::in_place, &skeleton);
			original->Play(walk);
			original->Update(0.2f);
			Animator copy = *original;
			original.reset();
			copy.Update(0.1f);
			const float gap = PoseGap(copy.GetLocalPoses(), PoseAt(skeleton, walk, 0.3f));
			Check(gap < 1e-5f, "a copied animator plays on after the original is gone");
		}

		if (Model* twin = Model::LoadFromFile(k_FoxPath))
		{
			Animator native(&skeleton);
			Animator retargeted(twin->GetSkeleton());
			native.Play(walk);
			retargeted.Play(walk);
			native.Update(0.37f);
			retargeted.Update(0.37f);

			float worst = 0.0f;
			for (uint32_t joint = 0; joint < skeleton.GetJointCount(); ++joint)
				worst = (std::max)(worst, glm::length(glm::vec3(native.GetJointTransform(joint)[3]) - glm::vec3(retargeted.GetJointTransform(joint)[3])));
			Check(worst < 1e-3f, std::format("Walk retargeted onto a second load of the Fox matches it played natively (worst {:.1e} of ~150 units)", worst));
			DestroyAndDelete(twin);
		}

		{
			std::vector<Joint> joints = skeleton.GetJoints();
			for (Joint& joint : joints)
				joint.RestPose.Translation *= 1.5f;
			const Skeleton tall(std::move(joints), skeleton.GetRootTransform(), skeleton.GetSkinJointCount());

			Animator native(&skeleton);
			Animator scaled(&tall);
			native.Play(walk);
			scaled.Play(walk);
			native.Update(0.37f);
			scaled.Update(0.37f);

			const int32_t hips = skeleton.FindJoint("b_Hip_01");
			float rotations = 0.0f;
			float lengths = 0.0f;
			for (uint32_t joint = 0; joint < skeleton.GetJointCount(); ++joint)
			{
				rotations = (std::max)(rotations, QuatGap(native.GetLocalPoses()[joint].Rotation, scaled.GetLocalPoses()[joint].Rotation));
				if (static_cast<int32_t>(joint) != hips)
					lengths = (std::max)(lengths, glm::length(scaled.GetLocalPoses()[joint].Translation - tall.GetJoint(joint).RestPose.Translation));
			}
			const float hipGap = hips == Skeleton::k_InvalidJoint ? 1.0f
				: glm::length(scaled.GetLocalPoses()[hips].Translation - 1.5f * native.GetLocalPoses()[hips].Translation);
			Check(rotations < 1e-6f && lengths < 1e-6f && hipGap < 1e-3f,
				std::format("Walk on a Fox 1.5x as long keeps its rotations, scales the hips' motion 1.5x and its own bone lengths ({:.1e}, {:.1e}, {:.1e})", rotations, hipGap, lengths));
		}

		{
			// The same rig in centimetres under a 0.01 root scale, as an FBX export carries it.
			std::vector<Joint> joints = skeleton.GetJoints();
			for (Joint& joint : joints)
				joint.RestPose.Translation *= 100.0f;
			const Skeleton centimetres(std::move(joints), skeleton.GetRootTransform() * glm::scale(glm::mat4(1.0f), glm::vec3(0.01f)), skeleton.GetSkinJointCount());

			Animator native(&skeleton);
			Animator retargeted(&centimetres);
			native.Play(walk);
			retargeted.Play(walk);
			native.Update(0.37f);
			retargeted.Update(0.37f);

			float worst = 0.0f;
			for (uint32_t joint = 0; joint < skeleton.GetJointCount(); ++joint)
				worst = (std::max)(worst, glm::length(glm::vec3(native.GetJointTransform(joint)[3]) - glm::vec3(retargeted.GetJointTransform(joint)[3])));
			Check(worst < 1e-2f, std::format("Walk on the Fox rebuilt in centimetres under a 0.01 root scale moves it the same in model space (worst {:.1e} of ~150 units)", worst));
		}
	}

	void AnimationTest::RunBlendChecks()
	{
		const Skeleton& skeleton = *m_Fox->GetSkeleton();
		const AnimationClip* survey = m_Fox->FindAnimation("Survey");
		const AnimationClip* walk = m_Fox->FindAnimation("Walk");
		const AnimationClip* run = m_Fox->FindAnimation("Run");
		const int32_t upperBody = skeleton.FindJoint(k_UpperBody);
		const int32_t neck = skeleton.FindJoint("b_Neck_04");
		if (!survey || !walk || !run || upperBody == Skeleton::k_InvalidJoint || neck == Skeleton::k_InvalidJoint)
			return;

		const AnimationState locomotion = AnimationState::Blend1D(k_SpeedParameter, { { 4.0f, run }, { 0.0f, survey }, { 1.5f, walk } });
		const float walkLength = walk->GetDuration();
		const float runLength = run->GetDuration();

		auto blendAt = [&](float speed, float phase)
		{
			Animator animator(&skeleton);
			animator.SetFloat(k_SpeedParameter, speed);
			animator.Play(locomotion);
			animator.SetNormalizedTime(phase);
			animator.Update(0.0f);
			return std::vector<JointPose>(animator.GetLocalPoses().begin(), animator.GetLocalPoses().end());
		};

		{
			const float onWalk = PoseGap(blendAt(1.5f, 0.4f), PoseAt(skeleton, walk, 0.4f * walkLength));
			std::vector<JointPose> expected = PoseAt(skeleton, walk, 0.4f * walkLength);
			Blend(expected, PoseAt(skeleton, run, 0.4f * runLength), 0.5f);
			const float halfway = PoseGap(blendAt(2.75f, 0.4f), expected);
			const float below = PoseGap(blendAt(-1.0f, 0.4f), PoseAt(skeleton, survey, 0.4f * survey->GetDuration()));
			const float above = PoseGap(blendAt(9.0f, 0.4f), PoseAt(skeleton, run, 0.4f * runLength));
			Check(onWalk < 1e-6f && halfway < 1e-6f && below < 1e-6f && above < 1e-6f,
				std::format("Blend1D on Speed: Walk at 1.5, half Walk and half Run at the same phase at 2.75, the end clips past either end ({:.1e}, {:.1e}, {:.1e}, {:.1e})", onWalk, halfway, below, above));
		}
		{
			Animator animator(&skeleton);
			animator.SetFloat(k_SpeedParameter, 2.75f);
			animator.Play(locomotion);
			const float cycle = 0.5f * (walkLength + runLength);
			animator.Update(0.25f * cycle);
			Check(std::abs(animator.GetNormalizedTime() - 0.25f) < 1e-5f && std::abs(animator.GetTime() - 0.25f * cycle) < 1e-5f && animator.GetFloat(k_SpeedParameter) == 2.75f && animator.GetFloat("Unset") == 0.0f,
				std::format("the 50/50 blend runs at the mean of the two cycles ({:.3f} s), both clips at the same fraction", cycle));
		}
		{
			Animator animator(&skeleton);
			animator.Play(locomotion);
			float worstStep = 0.0f;
			std::vector<glm::vec3> previous, positions(skeleton.GetJointCount());
			for (int step = 0; step <= 400; ++step)
			{
				animator.SetFloat(k_SpeedParameter, 0.01f * static_cast<float>(step));
				animator.SetNormalizedTime(0.3f);
				animator.Update(0.0f);
				for (uint32_t joint = 0; joint < skeleton.GetJointCount(); ++joint)
				{
					positions[joint] = glm::vec3(animator.GetJointTransform(joint)[3]);
					if (!previous.empty())
						worstStep = (std::max)(worstStep, glm::length(positions[joint] - previous[joint]));
				}
				previous = positions;
			}
			Check(worstStep < 1.5f, std::format("sweeping Speed from 0 to 4 in steps of 0.01 never moves a joint more than {:.2f} of ~150 units in a step", worstStep));
		}

		auto walkWithLayer = [&](const AnimationLayer& layer, const AnimationClip* clip)
		{
			Animator animator(&skeleton);
			animator.Play(walk);
			animator.SetLayer(1, layer);
			animator.Play(clip, 0.0f, 1);
			animator.SetTime(0.3f);
			animator.SetTime(0.3f, 1);
			animator.Update(0.0f);
			return std::vector<JointPose>(animator.GetLocalPoses().begin(), animator.GetLocalPoses().end());
		};
		auto insideMask = [&](int32_t joint, int32_t root)
		{
			for (int32_t j = joint; j >= 0; j = skeleton.GetJoint(j).Parent)
			{
				if (j == root)
					return true;
			}
			return false;
		};
		{
			const std::vector<JointPose> walkOnly = PoseAt(skeleton, walk, 0.3f);
			const std::vector<JointPose> surveyOnly = PoseAt(skeleton, survey, 0.3f);
			const std::vector<JointPose> full = walkWithLayer(AnimationLayer().SetMask(k_UpperBody), survey);
			const std::vector<JointPose> half = walkWithLayer(AnimationLayer().SetMask(k_UpperBody).SetWeight(0.5f), survey);
			const std::vector<JointPose> noNeck = walkWithLayer(AnimationLayer().SetMask(k_UpperBody).Exclude("b_Neck_04"), survey);

			float outside = 0.0f, inside = 0.0f, halfway = 0.0f, excluded = 0.0f;
			uint32_t insideCount = 0;
			for (uint32_t joint = 0; joint < skeleton.GetJointCount(); ++joint)
			{
				const std::span<const JointPose> one(&full[joint], 1);
				if (!insideMask(static_cast<int32_t>(joint), upperBody))
				{
					outside = (std::max)(outside, PoseGap(one, std::span<const JointPose>(&walkOnly[joint], 1)));
					continue;
				}

				insideCount++;
				inside = (std::max)(inside, PoseGap(one, std::span<const JointPose>(&surveyOnly[joint], 1)));
				std::vector<JointPose> expected{ walkOnly[joint] };
				Blend(expected, { surveyOnly[joint] }, 0.5f);
				halfway = (std::max)(halfway, PoseGap(std::span<const JointPose>(&half[joint], 1), expected));
				const bool underNeck = insideMask(static_cast<int32_t>(joint), neck);
				excluded = (std::max)(excluded, PoseGap(std::span<const JointPose>(&noNeck[joint], 1), std::span<const JointPose>(underNeck ? &walkOnly[joint] : &surveyOnly[joint], 1)));
			}
			Check(insideCount > 0 && outside == 0.0f && inside < 1e-6f && halfway < 1e-6f && excluded < 1e-6f,
				std::format("an upper-body layer from {} plays Survey on its {} joints over Walk, leaves the rest alone, goes halfway at weight 0.5 and skips an excluded neck", k_UpperBody, insideCount));

			// Survey animates every joint in the mask, so a clip of the head alone shows whether the
			// layer's other joints keep Walk rather than snapping to the rest pose.
			const int32_t head = skeleton.FindJoint(k_HatJoint);
			const AnimationChannel* headChannel = survey->FindChannel(k_HatJoint);
			if (head != Skeleton::k_InvalidJoint && headChannel)
			{
				const AnimationClip headOnly("Survey head", survey->GetDuration(), { *headChannel }, survey->GetSourceSkeleton());
				const std::vector<JointPose> headLayer = walkWithLayer(AnimationLayer().SetMask(k_UpperBody), &headOnly);
				float kept = 0.0f;
				for (uint32_t joint = 0; joint < skeleton.GetJointCount(); ++joint)
				{
					if (static_cast<int32_t>(joint) != head)
						kept = (std::max)(kept, PoseGap(std::span<const JointPose>(&headLayer[joint], 1), std::span<const JointPose>(&walkOnly[joint], 1)));
				}
				const float headGap = QuatGap(headLayer[head].Rotation, surveyOnly[head].Rotation);
				Check(kept == 0.0f && headGap < 1e-6f, "a layer clip that animates only the head turns the head and leaves the rest of its mask on Walk");
			}
		}
		{
			Animator animator(&skeleton);
			animator.Play(walk);
			animator.SetLayer(1, AnimationLayer().SetMask(k_UpperBody));
			animator.Update(0.2f);
			animator.Play(survey, 0.4f, 1);
			animator.Update(0.0f);
			const float start = PoseGap(animator.GetLocalPoses(), PoseAt(skeleton, walk, 0.2f));
			animator.Update(0.4f);
			const bool fadedIn = !animator.IsFading(1) && animator.GetCurrentClip(1) == survey;
			animator.Stop(0.2f, 1);
			animator.Update(0.2f);
			const float end = PoseGap(animator.GetLocalPoses(), PoseAt(skeleton, walk, 0.8f));
			Check(start < 1e-6f && fadedIn && end < 1e-4f,
				std::format("a layer fades in over the pose below and Stop fades it back out ({:.1e}, {:.1e})", start, end));
		}
		{
			// The way a script drives it: Walk played every frame, the one-shot once.
			Animator animator(&skeleton);
			animator.Play(walk);
			animator.Update(0.1f);
			animator.PlayOneShot(run, 0.1f, 0.2f);
			bool runShowed = false;
			for (int step = 0; step < 26; ++step)
			{
				animator.Play(walk);
				animator.Update(0.05f);
				if (step == 3)
					runShowed = animator.GetCurrentClip() == run && !animator.IsFading() && animator.IsOneShotPlaying();
			}
			const float walkTime = std::fmod(0.1f + 26 * 0.05f, walkLength);
			const float back = PoseGap(animator.GetLocalPoses(), PoseAt(skeleton, walk, walkTime));
			Check(runShowed && animator.GetCurrentClip() == walk && !animator.IsFading() && !animator.IsOneShotPlaying() && back < 1e-3f,
				std::format("a one-shot Run plays over Walk (played every frame) and fades back to it, Walk having run on in step ({:.1e})", back));

			animator.PlayOneShot(run, 0.1f, 0.2f);
			animator.Play(survey, 0.1f);
			for (int step = 0; step < 30; ++step)
				animator.Update(0.05f);
			Check(animator.GetCurrentClip() == survey, "a Play during a one-shot cancels its return");
		}
	}

	void AnimationTest::RunSceneChecks()
	{
		const AnimationClip* walk = m_Fox->FindAnimation("Walk");
		const AnimationClip* run = m_Fox->FindAnimation("Run");
		const int32_t head = m_Fox->GetSkeleton()->FindJoint(k_HatJoint);
		if (!walk || !run || head == Skeleton::k_InvalidJoint)
			return;

		Scene scene("Animation checks");
		Entity fox = scene.CreateEntity("Fox");
		fox.AddComponent<Transform3DComponent>(Transform3DComponent({ 2.0f, 0.0f, -1.0f }, glm::vec3(m_FoxScale)));
		fox.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_Fox));
		Check(scene.GetAnimator(fox) == nullptr, "no animator without an AnimatorComponent");
		fox.AddComponent<AnimatorComponent>(AnimatorComponent("Walk")).Speed = 2.0f;

		Entity hat = scene.CreateEntity("Hat");
		hat.AddComponent<Transform3DComponent>(Transform3DComponent(k_HatOffset, glm::vec3(k_HatSize)));
		hat.AddComponent<RigidBody3DComponent>().Type = BodyType3D::Kinematic;
		hat.SetParent(fox, k_HatJoint, false);
		Check(hat.GetParentJoint() == k_HatJoint, "GetParentJoint names the joint SetParent was given");

		scene.OnStart();
		scene.OnUpdate(0.05f);
		Animator* animator = scene.GetAnimator(fox);
		Check(animator && animator->GetCurrentClip() == walk && std::abs(animator->GetTime() - 0.1f) < 1e-5f,
			"an AnimatorComponent plays its DefaultClip, advanced by deltaTime x Speed");
		if (!animator)
			return;

		const glm::mat4 hatLocal = hat.GetComponent<Transform3DComponent>().GetTransform();
		auto hatGap = [&]()
		{
			const glm::mat4 expected = fox.GetWorldTransform() * SocketFrame(animator->GetJointTransform(head)) * hatLocal;
			return glm::length(hat.GetWorldPosition() - glm::vec3(expected[3]));
		};
		const glm::vec3 first = hat.GetWorldPosition();
		const float firstGap = hatGap();
		scene.OnUpdate(0.1f);
		const float secondGap = hatGap();
		const float moved = glm::length(hat.GetWorldPosition() - first);
		Check(firstGap < 1e-4f && secondGap < 1e-4f && moved > 1e-3f,
			std::format("a child on {} follows the joint as the Fox walks (moved {:.3f}, off by {:.1e})", k_HatJoint, moved, (std::max)(firstGap, secondGap)));

		Physics3D* physics = scene.GetPhysics3D();
		const float bodyGap = physics ? glm::length(physics->GetPosition(scene.GetRuntimeBody3D(hat)) - hat.GetWorldPosition()) : 1.0f;
		Check(bodyGap < 1e-4f, std::format("a kinematic body on the joint is driven to this frame's pose (off by {:.1e})", bodyGap));

		Entity copy = scene.DuplicateEntity(fox);
		const Entity copiedHat = copy.FindChild("Hat");
		Check(copy.HasComponent<AnimatorComponent>() && copiedHat && copiedHat.GetParentJoint() == k_HatJoint,
			"duplicating the Fox copies its AnimatorComponent and keeps the hat on its joint");

		AnimatorComponent& settings = fox.GetComponent<AnimatorComponent>();
		settings.Enabled = false;
		const float held = animator->GetTime();
		scene.OnUpdate(0.1f);
		Check(animator->GetTime() == held, "Enabled = false holds the pose");
		settings.Enabled = true;

		scene.OnStop();
		scene.OnStart();
		Check(scene.GetAnimator(fox) == animator && animator->GetTime() == held, "the animator survives OnStop and OnStart");

		const glm::vec3 before = hat.GetWorldPosition();
		hat.SetParent(fox);
		Check(hat.GetParentJoint().empty() && glm::length(hat.GetWorldPosition() - before) < 1e-4f,
			"SetParent without a joint takes the child off it and keeps its world position");

		Entity stray = scene.CreateEntity("Stray");
		stray.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.5f, 0.0f, 0.0f }));
		stray.SetParent(fox, "b_NoSuchJoint", false);
		const glm::vec3 strayExpected(fox.GetWorldTransform() * glm::vec4(0.5f, 0.0f, 0.0f, 1.0f));
		Check(glm::length(stray.GetWorldPosition() - strayExpected) < 1e-5f, "a joint the model lacks warns and leaves the child on the model's origin");

		fox.RemoveComponent<AnimatorComponent>();
		const bool freed = scene.GetAnimator(fox) == nullptr;
		fox.AddComponent<AnimatorComponent>(AnimatorComponent("Run"));
		Animator* fresh = scene.GetAnimator(fox);
		Check(freed && fresh && fresh->GetCurrentClip() == run && fresh->GetTime() == 0.0f,
			"removing the AnimatorComponent frees its animator, and a new one starts on its own DefaultClip");
	}

	void AnimationTest::BuildScene()
	{
		m_Scene = new Scene("Animation Test");
		Renderer3D& renderer = Application::Get().GetRenderer3D();

		Entity floor = m_Scene->CreateEntity("Floor");
		floor.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, -0.05f, 0.0f }, { 40.0f, 0.1f, 40.0f }));
		floor.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.42f, 0.46f, 0.40f, 1.0f }));

		const SubMesh* skinned = FindSkinnedSubMesh(*m_Fox);
		const std::string clipName = (m_Mode == Mode::Clip || m_Mode == Mode::Layers) && m_ClipName.empty() ? std::string("Walk")
			: m_Mode == Mode::Blend ? std::string() : m_ClipName;
		const bool animate = m_Mode == Mode::Blend || ((m_Mode == Mode::Clip || m_Mode == Mode::Layers || (m_Mode == Mode::Crowd && !m_CrowdStatic)) && !clipName.empty());
		m_AnimatedFox = {};
		std::vector<Entity> animated;
		const uint32_t count = m_Mode == Mode::Crowd ? m_CrowdCount : (m_Mode == Mode::Pose ? 0u : 1u);
		const uint32_t columns = (std::max)(1u, static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count)))));
		const uint32_t rows = (count + columns - 1) / columns;
		for (uint32_t i = 0; i < count; ++i)
		{
			const float x = (static_cast<float>(i % columns) - 0.5f * static_cast<float>(columns - 1)) * k_CrowdSpacing;
			const float z = (static_cast<float>(i / columns) - 0.5f * static_cast<float>(rows - 1)) * k_CrowdSpacing;

			Entity fox = m_Scene->CreateEntity(std::format("Fox {}", i));
			fox.AddComponent<Transform3DComponent>(Transform3DComponent(m_FoxOffset + glm::vec3(x, 0.0f, z), glm::vec3(m_FoxScale)));

			const bool drawStatic = m_Mode == Mode::BindStatic || (m_Mode == Mode::Crowd && m_CrowdStatic);
			if (drawStatic)
				fox.AddComponent<MeshRendererComponent>(MeshRendererComponent(skinned->MeshData)).Material = m_FoxMaterial;
			else
				fox.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_Fox)).Material = m_FoxMaterial;

			if (animate)
			{
				fox.AddComponent<AnimatorComponent>(AnimatorComponent(clipName));
				animated.push_back(fox);
			}

			if (m_Mode == Mode::Clip)
			{
				Entity hat = m_Scene->CreateEntity("Hat");
				hat.AddComponent<Transform3DComponent>(Transform3DComponent(k_HatOffset, glm::vec3(k_HatSize)));
				hat.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer.GetBoxMesh(), { 0.95f, 0.78f, 0.15f, 1.0f }));
				hat.SetParent(fox, k_HatJoint, false);
			}
		}

		// Out of step, so a crowd doesn't march in unison.
		for (size_t i = 0; i < animated.size(); ++i)
		{
			Animator* animator = m_Scene->GetAnimator(animated[i]);
			if (!animator)
				continue;

			m_AnimatedFox = animated[i];
			if (m_Mode == Mode::Blend)
			{
				animator->SetFloat(k_SpeedParameter, m_BlendSpeed);
				animator->Play(AnimationState::Blend1D(k_SpeedParameter,
					{ { 0.0f, m_Fox->FindAnimation("Survey") }, { 1.5f, m_Fox->FindAnimation("Walk") }, { 4.0f, m_Fox->FindAnimation("Run") } }));
				if (m_FreezePhase >= 0.0f)
				{
					animator->SetNormalizedTime(m_FreezePhase);
					animated[i].GetComponent<AnimatorComponent>().Enabled = false;
				}
			}
			else if (m_Mode == Mode::Layers)
			{
				animator->SetLayer(1, AnimationLayer().SetMask(k_UpperBody).SetWeight(m_LayerWeight));
				animator->Play(m_Fox->FindAnimation("Survey"), 0.0f, 1);
				if (m_FreezeTime >= 0.0f)
				{
					animator->SetTime(m_FreezeTime);
					animator->SetTime(m_FreezeTime, 1);
					animated[i].GetComponent<AnimatorComponent>().Enabled = false;
				}
			}
			else if (m_Mode == Mode::Clip && m_FreezeTime >= 0.0f)
			{
				animator->SetTime(m_FreezeTime);
				animated[i].GetComponent<AnimatorComponent>().Enabled = false;
			}
			else if (m_Mode == Mode::Crowd)
			{
				animator->SetTime(0.137f * static_cast<float>(i));
			}
			animator->Update(0.0f);
		}

		if (m_Mode == Mode::Pose)
		{
			const Skeleton& skeleton = *m_Fox->GetSkeleton();
			std::vector<JointPose> poses;
			for (const Joint& joint : skeleton.GetJoints())
				poses.push_back(joint.RestPose);

			auto bend = [&](const char* name, float degrees, const glm::vec3& axis)
			{
				const int32_t joint = skeleton.FindJoint(name);
				if (joint != Skeleton::k_InvalidJoint)
					poses[joint].Rotation = poses[joint].Rotation * Turn(degrees, axis);
			};
			bend("b_Neck_04", 25.0f, { 0.0f, 1.0f, 0.0f });
			bend("b_Head_05", 30.0f, { 0.0f, 1.0f, 0.0f });
			bend("b_Tail01_012", 30.0f, { 0.0f, 1.0f, 0.0f });
			bend("b_Tail02_013", 30.0f, { 0.0f, 1.0f, 0.0f });
			bend("b_Tail03_014", 30.0f, { 0.0f, 1.0f, 0.0f });
			bend("b_RightUpperArm_06", -40.0f, { 0.0f, 0.0f, 1.0f });
			bend("b_LeftLeg01_015", 35.0f, { 0.0f, 0.0f, 1.0f });

			std::vector<glm::mat4> globals(skeleton.GetJointCount());
			skeleton.ComputeGlobalTransforms(poses, globals);
			m_PosePalette.resize(skeleton.GetSkinJointCount());
			skeleton.ComputeSkinningPalette(globals, m_PosePalette);

			m_PoseTransform = glm::translate(glm::mat4(1.0f), m_FoxOffset) * glm::scale(glm::mat4(1.0f), glm::vec3(m_FoxScale));
		}

		m_Scene->OnStart();

		if (m_Mode == Mode::Crowd)
		{
			const float half = 0.5f * static_cast<float>(columns) * k_CrowdSpacing;
			m_Camera.SetPosition({ -0.9f * half - 2.0f, 1.1f * half + 1.5f, 1.4f * half + 2.5f });
			m_Camera.SetTarget({ 0.0f, 0.0f, 0.0f });
		}
		else
		{
			const float distance = 1.25f * m_FoxRadius / std::sin(glm::radians(0.5f * m_Camera.GetFOV()));
			m_Camera.SetPosition(m_FoxCenter + distance * glm::normalize(glm::vec3(-0.75f, 0.35f, 1.0f)));
			m_Camera.SetTarget(m_FoxCenter);
		}
	}

	void AnimationTest::DestroyScene()
	{
		delete m_Scene;
		m_Scene = nullptr;
	}

	void AnimationTest::SubmitPosedFox(Renderer3D& renderer)
	{
		const SubMesh* skinned = FindSkinnedSubMesh(*m_Fox);
		renderer.SubmitSkinnedMesh(skinned->MeshData, m_PoseTransform, m_PosePalette, glm::vec4(1.0f), m_FoxMaterial);
	}

	void AnimationTest::RunDrawChecks(const Renderer3D::Statistics& stats)
	{
		const uint32_t budget = Application::Get().GetRenderer3D().GetSkinnedInstanceBudget();
		switch (m_Mode)
		{
			case Mode::Bind:
				Check(stats.SkinnedDraws == 1 && stats.SkinnedInstances == 1 && stats.SkinnedJoints == 24 && stats.DroppedSkinnedDraws == 0, "the bind-pose Fox is one skinned draw of 24 joints");
				break;
			case Mode::BindStatic:
				Check(stats.SkinnedDraws == 0, "the static Fox takes the batched path");
				break;
			case Mode::Pose:
				Check(stats.SkinnedDraws == 1 && stats.SkinnedJoints == 24 && stats.DroppedSkinnedDraws == 0, "the posed Fox is one skinned draw of 24 joints");
				break;
			case Mode::Blend:
				Check(stats.SkinnedDraws == 1 && stats.SkinnedJoints == 24 && stats.DroppedSkinnedDraws == 0, "the blended Fox is one skinned draw of 24 joints");
				break;
			case Mode::Layers:
				Check(stats.SkinnedDraws == 1 && stats.SkinnedJoints == 24 && stats.DroppedSkinnedDraws == 0, "the layered Fox is one skinned draw of 24 joints");
				break;
			case Mode::Clip:
				Check(stats.SkinnedDraws == 1 && stats.SkinnedInstances == 1 && stats.SkinnedJoints == 24 && stats.DroppedSkinnedDraws == 0, "the animated Fox is one skinned draw of 24 joints");
				break;
			case Mode::Crowd:
				if (m_CrowdStatic)
				{
					Check(stats.SkinnedDraws == 0, std::format("{} static foxes take the batched path", m_CrowdCount));
					break;
				}
				const uint32_t expected = (std::min)(m_CrowdCount, budget);
				Check(stats.SkinnedDraws == expected && stats.DroppedSkinnedDraws == m_CrowdCount - expected,
					std::format("{} foxes: {} skinned draws and {} dropped past MaxSkinnedInstances ({})", m_CrowdCount, expected, m_CrowdCount - expected, budget));
				break;
		}
	}

	void AnimationTest::TrackTiming(float deltaTime, double updateMs, double renderMs, double endSceneMs)
	{
		m_Time += deltaTime;
		if (m_Mode != Mode::Crowd || m_Time < k_WarmupSeconds || m_TimedFrames >= k_MeasuredFrames)
			return;

		m_FrameMs += deltaTime * 1000.0;
		m_UpdateMs += updateMs;
		m_RenderMs += renderMs;
		m_EndSceneMs += endSceneMs;
		if (++m_TimedFrames < k_MeasuredFrames)
			return;

		const double frames = static_cast<double>(m_TimedFrames);
		const std::string kind = m_CrowdStatic ? std::string("static") : m_ClipName.empty() ? std::string("skinned") : std::format("skinned, playing {}", m_ClipName);
		m_TimingResult = std::format("{} foxes, {}: frame {:.2f} ms, Scene::OnUpdate {:.3f} ms, RenderEntities3D {:.3f} ms, EndScene recording {:.3f} ms (mean of {} frames)",
			m_CrowdCount, kind, m_FrameMs / frames, m_UpdateMs / frames, m_RenderMs / frames, m_EndSceneMs / frames, m_TimedFrames);
		DE_INFO("[Anim] {}", m_TimingResult);
	}

	void AnimationTest::Update(float deltaTime)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		renderer.BeginScene(m_Camera);
		renderer.Clear(m_ClearColor);
		if (!m_Scene)
		{
			renderer.EndScene();
			return;
		}

		if (Animator* animator = (m_Mode == Mode::Blend || m_Mode == Mode::Layers) && m_AnimatedFox ? m_Scene->GetAnimator(m_AnimatedFox) : nullptr)
		{
			animator->SetFloat(k_SpeedParameter, m_BlendSpeed);
			if (m_Mode == Mode::Layers)
				animator->SetLayerWeight(1, m_LayerWeight);
			// A frozen Fox isn't advanced, so the sliders still show at once.
			if (!m_AnimatedFox.GetComponent<AnimatorComponent>().Enabled)
				animator->Update(0.0f);
		}

		const Clock::time_point updateStart = Clock::now();
		m_Scene->OnUpdate((std::min)(deltaTime, 1.0f / 30.0f));
		const Clock::time_point updateEnd = Clock::now();
		m_Scene->SubmitLights(renderer);

		const Clock::time_point renderStart = Clock::now();
		m_Scene->RenderEntities3D(renderer);
		if (m_Mode == Mode::Pose)
			SubmitPosedFox(renderer);
		const Clock::time_point endSceneStart = Clock::now();
		renderer.EndScene();
		const Clock::time_point endSceneEnd = Clock::now();

		m_LastStats = renderer.GetStatistics();
		if (!m_DrawChecksDone)
		{
			m_DrawChecksDone = true;
			RunDrawChecks(m_LastStats);
		}
		TrackTiming(deltaTime, Milliseconds(updateStart, updateEnd), Milliseconds(renderStart, endSceneStart), Milliseconds(endSceneStart, endSceneEnd));
	}

	void AnimationTest::Cleanup()
	{
		DestroyScene();
		DestroyAndDelete(m_FoxMaterial);
		DestroyAndDelete(m_Fox);
		m_PosePalette.clear();
	}

	void AnimationTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void AnimationTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();
		ImGui::Separator();

		int mode = static_cast<int>(m_Mode);
		ImGui::RadioButton("Bind", &mode, static_cast<int>(Mode::Bind));
		ImGui::SameLine();
		ImGui::RadioButton("Bind (static)", &mode, static_cast<int>(Mode::BindStatic));
		ImGui::SameLine();
		ImGui::RadioButton("Pose", &mode, static_cast<int>(Mode::Pose));
		ImGui::SameLine();
		ImGui::RadioButton("Clip", &mode, static_cast<int>(Mode::Clip));
		ImGui::SameLine();
		ImGui::RadioButton("Blend", &mode, static_cast<int>(Mode::Blend));
		ImGui::SameLine();
		ImGui::RadioButton("Layers", &mode, static_cast<int>(Mode::Layers));
		ImGui::SameLine();
		ImGui::RadioButton("Crowd", &mode, static_cast<int>(Mode::Crowd));
		if (mode != static_cast<int>(m_Mode))
		{
			m_Mode = static_cast<Mode>(mode);
			Cleanup();
			Initialize();
			return;
		}

		ImGui::Text("Skinned draws %u, instances %u, dropped %u, joints %u, draw calls %u",
			m_LastStats.SkinnedDraws, m_LastStats.SkinnedInstances, m_LastStats.DroppedSkinnedDraws, m_LastStats.SkinnedJoints, m_LastStats.DrawCalls);
		if (m_Mode == Mode::Crowd)
			ImGui::TextWrapped("Timing: %s", m_TimingResult.empty() ? "measuring..." : m_TimingResult.c_str());
		if (m_Mode == Mode::Blend)
			ImGui::SliderFloat("Speed (Survey 0, Walk 1.5, Run 4)", &m_BlendSpeed, 0.0f, 4.0f);
		if (m_Mode == Mode::Layers)
		{
			ImGui::SliderFloat("Upper-body Survey weight", &m_LayerWeight, 0.0f, 1.0f);
			Animator* animator = m_Scene && m_AnimatedFox ? m_Scene->GetAnimator(m_AnimatedFox) : nullptr;
			if (animator && ImGui::Button("One-shot Run"))
				animator->PlayOneShot(m_Fox->FindAnimation("Run"), 0.15f, 0.25f);
		}

		ImGui::Separator();
		for (const CheckResult& check : m_Checks)
		{
			const ImVec4 color = check.Passed ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
			ImGui::TextColored(color, "%s %s", check.Passed ? "[PASS]" : "[FAIL]", check.Name.c_str());
		}
	}

}
