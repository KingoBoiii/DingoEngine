#include "AnimationTest.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
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

	// A custom skinned shader, as docs/animation.md describes one.
	constexpr const char* k_GhostShader = R"(
#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in uvec4 a_Joints;
layout(location = 4) in vec4 a_Weights;

layout(std140, binding = 0) uniform CameraData
{
    mat4 ViewProjection;
    vec4 LightDirection;
    vec4 Ambient;
};

layout(std140, binding = 2) uniform SkinData
{
    mat4 Model;
    mat4 NormalMatrix;
    vec4 Color;
    mat4 Joints[128];
};

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_TexCoord;

void main()
{
    mat4 skin = a_Weights.x * Joints[a_Joints.x] + a_Weights.y * Joints[a_Joints.y]
              + a_Weights.z * Joints[a_Joints.z] + a_Weights.w * Joints[a_Joints.w];
    gl_Position = ViewProjection * Model * skin * vec4(a_Position, 1.0);
    v_Color = Color;
    v_Normal = mat3(NormalMatrix) * a_Normal;
    v_TexCoord = a_TexCoord;
}

#type fragment
#version 450

layout(location = 0) in vec4 v_Color;
layout(location = 0) out vec4 o_Color;

void main()
{
    o_Color = v_Color;
}
)";

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

	class EventListener : public ScriptableEntity
	{
	public:
		explicit EventListener(std::function<void(Entity, const AnimationEvent&)> onEvent) : m_OnEvent(std::move(onEvent)) {}

	protected:
		void OnAnimationEvent(const AnimationEvent& event) override { m_OnEvent(GetEntity(), event); }

	private:
		std::function<void(Entity, const AnimationEvent&)> m_OnEvent;
	};

	class UpdateHook : public ScriptableEntity
	{
	public:
		explicit UpdateHook(std::function<void(float)> onUpdate) : m_OnUpdate(std::move(onUpdate)) {}

	protected:
		void OnUpdate(float deltaTime) override { m_OnUpdate(deltaTime); }

	private:
		std::function<void(float)> m_OnUpdate;
	};

	int CountEvents(std::span<const AnimationEvent> events, std::string_view name, AnimationEventType type)
	{
		return static_cast<int>(std::count_if(events.begin(), events.end(), [&](const AnimationEvent& event) { return event.Name == name && event.Type == type; }));
	}

	// The foot each of Fox.events' step events marks.
	const char* StepJoint(std::string_view event)
	{
		if (event == "step_fl") return "b_LeftHand_011";
		if (event == "step_fr") return "b_RightHand_08";
		if (event == "step_bl") return "b_LeftFoot01_017";
		if (event == "step_br") return "b_RightFoot01_021";
		return nullptr;
	}

	std::string ReadText(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary);
		return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	}

	bool WriteText(const std::filesystem::path& path, std::string_view text)
	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(text.data(), static_cast<std::streamsize>(text.size()));
		return static_cast<bool>(file);
	}

	std::string Replace(std::string text, std::string_view from, std::string_view to)
	{
		for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
			text.replace(at, from.size(), to);
		return text;
	}

	// The clip object called Walk gets Run's keys, as if an artist had re-exported it.
	std::string SwapWalkAndRun(std::string gltf)
	{
		gltf = Replace(std::move(gltf), "\"name\": \"Walk\"", "\"name\": \"@swap@\"");
		gltf = Replace(std::move(gltf), "\"name\": \"Run\"", "\"name\": \"Walk\"");
		return Replace(std::move(gltf), "\"name\": \"@swap@\"", "\"name\": \"Run\"");
	}

	// The reload checks edit a copy, never the test's own Fox. Empty when the copy fails.
	std::filesystem::path CopyFox(std::string_view folder)
	{
		const std::filesystem::path source = std::filesystem::path(k_FoxPath).parent_path();
		const std::filesystem::path target = std::filesystem::temp_directory_path() / "DingoAnimationTest" / folder;
		std::error_code error;
		std::filesystem::remove_all(target, error);
		std::filesystem::create_directories(target, error);
		for (const char* file : { "Fox.gltf", "Fox.bin", "Texture.png", "Fox.events" })
		{
			if (!std::filesystem::copy_file(source / file, target / file, std::filesystem::copy_options::overwrite_existing, error))
				return {};
		}
		return target / "Fox.gltf";
	}

	// The unit box stretched from one point to another, `thickness` across.
	glm::mat4 BoneBox(const glm::vec3& from, const glm::vec3& to, float thickness)
	{
		const glm::vec3 axis = to - from;
		const float length = glm::length(axis);
		const glm::vec3 y = length > 1e-6f ? axis / length : glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::vec3 helper = std::abs(y.z) < 0.9f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
		const glm::vec3 x = glm::normalize(glm::cross(helper, y));
		const glm::vec3 z = glm::cross(x, y);

		glm::mat4 box(1.0f);
		box[0] = glm::vec4(x * thickness, 0.0f);
		box[1] = glm::vec4(y * length, 0.0f);
		box[2] = glm::vec4(z * thickness, 0.0f);
		box[3] = glm::vec4(0.5f * (from + to), 1.0f);
		return box;
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
		m_SkinnedMaterialChecked = false;
		m_Time = 0.0f;
		m_TimedFrames = 0;
		m_FrameMs = m_UpdateMs = m_RenderMs = m_EndSceneMs = 0.0;
		m_EventLog.clear();
		m_Footprints.clear();
		m_TimingResult.clear();

		if (!m_ArgsRead)
		{
			m_ArgsRead = true;
			const ApplicationCommandLineArgs& args = Application::Get().GetCommandLineArgs();
			if (auto mode = args.Get("anim"))
			{
				m_Mode = *mode == "bindstatic" ? Mode::BindStatic : *mode == "pose" ? Mode::Pose
					: *mode == "clip" ? Mode::Clip : *mode == "blend" ? Mode::Blend : *mode == "layers" ? Mode::Layers
					: *mode == "events" ? Mode::Events : *mode == "crowd" ? Mode::Crowd : Mode::Bind;
				if (m_Mode == Mode::Bind && *mode != "bind")
					DE_WARN("Animation Test: unknown --anim={}; showing bind. Use bind, bindstatic, pose, clip, blend, layers, events or crowd.", *mode);
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
			m_LiveReload = args.Get("anim-reload").has_value();
			m_ShowSkeleton = args.Get("anim-skeleton").has_value();
		}

		m_Camera = PerspectiveCamera(45.0f, m_AspectRatio, 0.05f, 200.0f);

		m_ReloadStep = 0;
		if (m_LiveReload && m_Mode == Mode::Clip)
		{
			AssetManager& assets = Application::Get().GetAssetManager();
			const std::filesystem::path path = CopyFox("live");
			m_FoxAsset = path.empty() ? k_InvalidAsset : assets.Load(path);
			m_Fox = assets.GetModel(m_FoxAsset);
			if (m_FoxAsset != k_InvalidAsset)
			{
				m_HotReloadWas = assets.IsHotReloadEnabled();
				assets.SetHotReloadEnabled(true);
			}
		}
		else
		{
			m_Fox = Model::LoadFromFile(k_FoxPath);
		}
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
		RunEventChecks();
		RunSceneChecks();
		RunReloadChecks();

		m_RunPose.clear();
		if (const AnimationClip* run = m_FoxAsset != k_InvalidAsset ? m_Fox->FindAnimation("Run") : nullptr)
		{
			m_RunDuration = run->GetDuration();
			if (m_FreezeTime >= 0.0f)
				m_RunPose = PoseAt(*m_Fox->GetSkeleton(), run, m_FreezeTime);
		}

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
		{
			Animator animator(&skeleton);
			animator.SetLayer(1, AnimationLayer().SetMask(k_UpperBody));
			animator.PlayOneShot(run, 0.1f, 0.2f, 1);
			animator.Update(0.1f);
			animator.Stop(0.1f, 1);
			Check(!animator.IsOneShotPlaying(1) && !animator.GetCurrentClip(1), "Stop cancels a one-shot on a layer that was empty");
		}
		{
			Animator animator(&skeleton);
			animator.Play(walk);
			animator.SetLayer((std::numeric_limits<uint32_t>::max)(), AnimationLayer());
			animator.SetLayerWeight(1000000000u, 0.5f);
			animator.Play(run, 0.0f, (std::numeric_limits<uint32_t>::max)());
			animator.PlayOneShot(run, 0.1f, 0.2f, 4000000000u);
			animator.Update(0.1f);
			Check(animator.GetLayerCount() == 1 && animator.GetCurrentClip() == walk, "a layer index past the 32 an animator can have is ignored, not made");
		}

		{
			Animator animator(&skeleton);
			animator.Play(walk);
			animator.Update(0.1f);
			animator.Play(run, 0.4f);
			animator.Update(0.2f);
			const std::vector<AnimatorStateInfo> fading = animator.GetStates();
			const bool crossFade = fading.size() == 2 && fading[0].Clip == walk && fading[1].Clip == run && fading[0].Weight == 1.0f
				&& std::abs(fading[1].Weight - 0.5f) < 1e-5f && std::abs(fading[1].NormalizedTime - 0.2f / run->GetDuration()) < 1e-5f;

			Check(crossFade && animator.GetStates(3).empty(), "GetStates reports a cross-fade as both clips, the incoming one at its fade weight and time");

			animator.Play(locomotion);
			animator.SetFloat(k_SpeedParameter, 2.0f);
			animator.Update(0.1f);
			const std::vector<AnimatorStateInfo> walking = animator.GetStates();
			animator.SetFloat(k_SpeedParameter, 3.5f);
			const std::vector<AnimatorStateInfo> running = animator.GetStates();
			const bool blend = walking.size() == 1 && walking[0].Blend && walking[0].Parameter == k_SpeedParameter && walking[0].Clip == walk
				&& walking[0].Weight == 1.0f && walking[0].NormalizedTime == walking[0].Time && walking[0].Time > 0.0f
				&& running.size() == 1 && running[0].Clip == run;
			Check(blend, "GetStates reports a blend by its parameter and heavier clip (Walk at Speed 2, Run at 3.5) and its shared phase");

			for (int i = 0; i < 5; ++i)
				animator.Play(i % 2 == 0 ? walk : run, 1.0f);
			const std::vector<AnimatorStateInfo> crowded = animator.GetStates();
			Check(!crowded.empty() && crowded.front().Frozen && !crowded.front().Clip && crowded.size() <= 4,
				std::format("past four states GetStates shows the held mix first ({} states)", crowded.size()));
		}
	}

	void AnimationTest::RunEventChecks()
	{
		const Skeleton& skeleton = *m_Fox->GetSkeleton();
		const AnimationClip* walk = m_Fox->FindAnimation("Walk");
		const AnimationClip* run = m_Fox->FindAnimation("Run");
		const AnimationClip* survey = m_Fox->FindAnimation("Survey");
		if (!walk || !run || !survey)
			return;

		auto steps = [](const AnimationClip& clip)
		{
			return std::count_if(clip.GetEvents().begin(), clip.GetEvents().end(), [](const AnimationClipEvent& event) { return !event.Range && StepJoint(event.Name); });
		};
		Check(steps(*walk) == 4 && steps(*run) == 4 && survey->GetEvents().size() == 1 && survey->GetEvents()[0].Range && survey->GetEvents()[0].Name == "look",
			"Fox.events beside the model gives Walk and Run four footfalls each and Survey a range");

		// Clips of events alone: no channels, so they pose the rest pose.
		{
			AnimationChannel head;
			head.JointName = k_HatJoint;
			head.Rotation.Times = { 0.0f, 0.5f, 1.0f };
			head.Rotation.Values = { glm::quat(1.0f, 0.0f, 0.0f, 0.0f), Turn(40.0f, { 0.0f, 1.0f, 0.0f }) };
			AnimationClip ragged("Ragged", 1.0f, { head }, &skeleton);
			Animator animator(&skeleton);
			animator.Play(&ragged);
			animator.SetTime(0.9f);
			animator.Update(0.0f);
			const glm::quat pose = animator.GetLocalPoses()[skeleton.FindJoint(k_HatJoint)].Rotation;
			Check(ragged.GetChannels()[0].Rotation.Times.size() == 2 && std::isfinite(pose.w),
				"a hand-built track with more times than values is trimmed rather than read past its end");
		}

		AnimationClip loop("Loop", 1.0f, {}, &skeleton);
		loop.AddEvent(0.55f, "mid");
		loop.AddEventRange(0.25f, 0.45f, "window");
		loop.AddEvent(0.0f, "start");
		AnimationClip other("Other", 1.0f, {}, &skeleton);

		{
			Animator animator(&skeleton);
			animator.Play(&loop);
			int starts = 0, mids = 0, opens = 0, closes = 0;
			bool openAt03 = false, shutAt05 = false;
			for (int step = 0; step <= 26; ++step)
			{
				animator.Update(step == 0 ? 0.0f : 0.1f);
				const std::span<const AnimationEvent> events = animator.GetEventsThisFrame();
				starts += CountEvents(events, "start", AnimationEventType::Instant);
				mids += CountEvents(events, "mid", AnimationEventType::Instant);
				opens += CountEvents(events, "window", AnimationEventType::RangeBegin);
				closes += CountEvents(events, "window", AnimationEventType::RangeEnd);
				if (step == 3)
					openAt03 = animator.IsEventActive("window");
				if (step == 5)
					shutAt05 = !animator.IsEventActive("window");
			}
			Check(starts == 3 && mids == 3 && opens == 3 && closes == 3 && openAt03 && shutAt05,
				std::format("over 2.6 loops a clip fires its start (on the first Update too), an instant and a range once per loop ({}, {}, {}/{})", starts, mids, opens, closes));
		}
		{
			Animator animator(&skeleton);
			animator.Play(&loop);
			animator.Update(0.0f);
			animator.Update(0.3f);
			const bool open = animator.IsEventActive("window");
			animator.Play(&other, 0.2f);
			animator.Update(0.05f);
			const bool stillOpen = animator.IsEventActive("window");
			animator.Update(0.06f);
			const bool closedEarly = !animator.IsEventActive("window") && CountEvents(animator.GetEventsThisFrame(), "window", AnimationEventType::RangeEnd) == 1;
			Check(open && stillOpen && closedEarly, "a range stays open while its clip leads a cross-fade and closes the moment the incoming clip passes half, before its own end");
		}
		{
			AnimationClip swing("Swing", 1.0f, {}, &skeleton);
			swing.AddEventRange(0.6f, 0.95f, "hitbox");
			Animator animator(&skeleton);
			animator.Play(&other);
			animator.Update(0.0f);
			animator.PlayOneShot(&swing, 0.1f, 0.2f);
			int openedAt = -1, closedAt = -1;
			for (int step = 0; step < 24; ++step)
			{
				animator.Update(0.05f);
				if (CountEvents(animator.GetEventsThisFrame(), "hitbox", AnimationEventType::RangeBegin))
					openedAt = step;
				if (CountEvents(animator.GetEventsThisFrame(), "hitbox", AnimationEventType::RangeEnd))
					closedAt = step;
			}
			// Steps of 0.05 s: the range opens at 0.6 s (step 11, or 12 as the sum rounds); its own
			// end, 0.95 s, would be step 18.
			Check(openedAt >= 11 && openedAt <= 12 && closedAt > openedAt && closedAt < 18,
				std::format("a one-shot's hitbox opens at 0.6 s and closes as the one-shot starts fading back (step {}), not at its own end", closedAt));

			AnimationClip early("Early", 1.0f, {}, &skeleton);
			early.AddEventRange(0.0f, 0.3f, "hitbox");
			Animator cancelled(&skeleton);
			cancelled.Play(&other);
			cancelled.Update(0.0f);
			cancelled.PlayOneShot(&early, 0.5f, 0.2f);
			int opened = 0;
			cancelled.Update(0.1f);
			opened += CountEvents(cancelled.GetEventsThisFrame(), "hitbox", AnimationEventType::RangeBegin);
			cancelled.Play(&loop, 0.1f);
			for (int step = 0; step < 10; ++step)
			{
				cancelled.Update(0.05f);
				opened += CountEvents(cancelled.GetEventsThisFrame(), "hitbox", AnimationEventType::RangeBegin);
			}
			Check(opened == 0, "a swing cancelled before it was half faded in never opens its hitbox");
		}
		{
			// Two gaits authored in step: a footfall at a quarter and three quarters of each cycle.
			AnimationClip slow("Slow", 0.7f, {}, &skeleton);
			slow.AddEvent(0.175f, "step");
			slow.AddEvent(0.525f, "step");
			AnimationClip fast("Fast", 1.1f, {}, &skeleton);
			fast.AddEvent(0.275f, "step");
			fast.AddEvent(0.825f, "step");

			Animator animator(&skeleton);
			animator.SetFloat(k_SpeedParameter, 1.5f);
			animator.Play(AnimationState::Blend1D(k_SpeedParameter, { { 1.5f, &slow }, { 4.0f, &fast } }));
			animator.Update(0.0f);
			int fired = 0, expected = 0;
			for (int frame = 0; frame < 600; ++frame)
			{
				animator.SetFloat(k_SpeedParameter, 1.5f + 2.5f * static_cast<float>(frame) / 599.0f);
				const float before = animator.GetNormalizedTime();
				animator.Update(1.0f / 60.0f);
				const float after = animator.GetNormalizedTime();
				for (float footfall : { 0.25f, 0.75f })
					expected += after >= before ? (footfall > before && footfall <= after) : (footfall > before || footfall <= after);
				fired += CountEvents(animator.GetEventsThisFrame(), "step", AnimationEventType::Instant);
			}
			Check(expected > 10 && fired == expected,
				std::format("a Speed sweep from one gait to the other over 10 s fires every footfall exactly once ({} of {})", fired, expected));
		}
		{
			Animator animator(&skeleton);
			animator.Play(AnimationState::Clip(&loop).SetSpeed(-1.0f));
			animator.Update(0.0f);
			const bool quietStart = animator.GetEventsThisFrame().empty();
			animator.Update(0.6f);
			const bool open = animator.IsEventActive("window");
			animator.Update(0.2f);
			Check(quietStart && open && !animator.IsEventActive("window"),
				"played backwards, a range opens at its end and closes at its start, and Update(0) at the start fires nothing");
		}
		{
			Animator animator(&skeleton);
			animator.Play(&loop);
			animator.Update(0.0f);
			animator.Update(0.1f);
			animator.Update(1.2f);
			const std::span<const AnimationEvent> events = animator.GetEventsThisFrame();
			Check(CountEvents(events, "mid", AnimationEventType::Instant) == 1 && CountEvents(events, "start", AnimationEventType::Instant) == 1
				&& CountEvents(events, "window", AnimationEventType::RangeBegin) == 2 && animator.IsEventActive("window"),
				"a step longer than the loop fires what it passed on the way round, and the window it ends in is open");
		}
		{
			AnimationClip blink("Blink", 1.0f, {}, &skeleton);
			blink.AddEventRange(0.5f, 0.5f, "blink");
			Animator animator(&skeleton);
			animator.Play(&blink);
			animator.Update(0.0f);
			animator.Update(0.6f);
			const std::span<const AnimationEvent> events = animator.GetEventsThisFrame();
			const bool ordered = events.size() == 2 && events[0].Type == AnimationEventType::RangeBegin && events[1].Type == AnimationEventType::RangeEnd;
			Check(ordered && !animator.IsEventActive("blink"), "a range of zero length opens and closes as playback crosses it");
		}
		{
			AnimationClip shot("Shot", 0.5f, {}, &skeleton);
			shot.AddEvent(0.0f, "whoosh");
			shot.AddEvent(0.5f, "done");
			Animator animator(&skeleton);
			animator.Play(&other);
			animator.Update(0.0f);
			animator.PlayOneShot(&shot, 0.1f, 0.0f);
			int whoosh = 0, done = 0;
			for (int step = 0; step < 20; ++step)
			{
				animator.Update(0.05f);
				whoosh += CountEvents(animator.GetEventsThisFrame(), "whoosh", AnimationEventType::Instant);
				done += CountEvents(animator.GetEventsThisFrame(), "done", AnimationEventType::Instant);
			}
			Check(whoosh == 1 && done == 1, "a one-shot fires its first mark though it faded in, and its last though it cuts straight back");
		}
		{
			// Short enough to start back in its first frame, while Walk still leads.
			AnimationClip flinch("Flinch", 0.2f, {}, &skeleton);
			Animator animator(&skeleton);
			animator.Play(walk);
			animator.SetTime(0.28f);
			animator.Update(0.0f);
			animator.PlayOneShot(&flinch, 0.1f, 0.2f);
			animator.Update(1.0f / 60.0f);
			const int steps = CountEvents(animator.GetEventsThisFrame(), "step_fl", AnimationEventType::Instant);
			Check(steps == 1, std::format("a one-shot that starts back before it leads doesn't fire Walk's footfall again ({} step_fl)", steps));
		}
		{
			AnimationClip slash("Slash", 1.0f, {}, &skeleton);
			slash.AddEvent(0.12f, "whoosh");
			slash.AddEventRange(0.32f, 0.48f, "hitbox");
			Animator seeked(&skeleton);
			seeked.Play(AnimationState::Clip(&slash).SetLoop(false));
			seeked.SetTime(0.4f);
			seeked.Update(0.05f);
			const bool fromSeek = seeked.GetEventsThisFrame().empty() && !seeked.IsEventActive("hitbox");

			Animator reversed(&skeleton);
			reversed.Play(AnimationState::Clip(&slash).SetLoop(false));
			reversed.Update(-0.05f);
			Check(fromSeek && reversed.GetEventsThisFrame().empty(),
				std::format("a swing seeked to 0.4 s before it ever played fires nothing behind it, and one stepped backwards from its start fires nothing ({} events)", reversed.GetEventsThisFrame().size()));
		}
		{
			AnimationClip slash("Slash", 1.0f, {}, &skeleton);
			slash.AddEventRange(0.32f, 0.48f, "hitbox");
			Animator animator(&skeleton);
			animator.Play(AnimationState::Clip(&slash).SetLoop(false));
			animator.Update(0.4f);
			const bool open = animator.IsEventActive("hitbox");
			animator.SetTime(0.0f);
			animator.Update(0.1f);
			const bool closedBySeek = !animator.IsEventActive("hitbox") && CountEvents(animator.GetEventsThisFrame(), "hitbox", AnimationEventType::RangeEnd) == 1;
			animator.Update(0.3f);
			Check(open && closedBySeek && animator.IsEventActive("hitbox"), "SetTime(0) mid-swing closes the hitbox, and the replay opens it again");
		}
		{
			Animator animator(&skeleton);
			animator.Play(&other);
			animator.SetLayer(1, AnimationLayer().SetWeight(0.3f));
			animator.Play(&loop, 0.0f, 1);
			animator.Update(0.0f);
			animator.Update(0.6f);
			const bool quietBelowHalf = animator.GetEventsThisFrame().empty();
			animator.SetLayerWeight(1, 1.0f);
			animator.Update(0.6f);
			Check(quietBelowHalf && CountEvents(animator.GetEventsThisFrame(), "start", AnimationEventType::Instant) == 1 && animator.GetEventsThisFrame()[0].Layer == 1,
				"a layer above 0 fires nothing below weight 0.5 and its own events at full weight");
		}
		{
			Scene scene("Event checks");
			Entity fox = scene.CreateEntity("Fox");
			fox.AddComponent<Transform3DComponent>(Transform3DComponent(glm::vec3(0.0f), glm::vec3(m_FoxScale)));
			fox.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_Fox));
			fox.AddComponent<AnimatorComponent>(AnimatorComponent("Walk"));
			Entity target = scene.CreateEntity("Target");

			int heard = 0;
			bool validInside = false;
			fox.AddScript<EventListener>([&](Entity, const AnimationEvent&)
			{
				heard++;
				if (target.IsValid())
				{
					scene.DestroyEntity(target);
					validInside = target.IsValid();
				}
			});
			scene.OnStart();

			Animator reference(&skeleton);
			reference.Play(walk);
			int expected = 0;
			for (int frame = 0; frame < 60; ++frame)
			{
				scene.OnUpdate(1.0f / 60.0f);
				reference.Update(1.0f / 60.0f);
				expected += static_cast<int>(reference.GetEventsThisFrame().size());
			}
			Check(expected > 0 && heard == expected && validInside && !target.IsValid(),
				std::format("a script hears each of Walk's {} footfalls in a second, and a DestroyEntity from OnAnimationEvent waits for the end of the pass", heard));
		}
		if (Model* roaring = Model::LoadFromFile(k_FoxPath))
		{
			// A clip whose first mark is its very start, on an entity spawned by a script mid-frame:
			// its script only starts next frame, and must still hear it.
			roaring->FindAnimation("Walk")->AddEvent(0.0f, "roar");
			Scene scene("Spawn checks");
			int roars = 0;
			bool spawned = false;
			Entity spawner = scene.CreateEntity("Spawner");
			spawner.AddScript<UpdateHook>([&](float)
			{
				if (spawned)
					return;
				spawned = true;
				Entity fox = scene.CreateEntity("Spawned Fox");
				fox.AddComponent<Transform3DComponent>(Transform3DComponent(glm::vec3(0.0f), glm::vec3(m_FoxScale)));
				fox.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(roaring));
				fox.AddComponent<AnimatorComponent>(AnimatorComponent("Walk"));
				fox.AddScript<EventListener>([&](Entity, const AnimationEvent& event) { roars += event.Name == "roar"; });
			});
			scene.OnStart();
			for (int frame = 0; frame < 5; ++frame)
				scene.OnUpdate(1.0f / 60.0f);
			Check(spawned && roars == 1, "an entity a script spawns mid-frame hears its clip's first mark once its script has started");
			scene.Clear();
			DestroyAndDelete(roaring);
		}
	}

	void AnimationTest::RunReloadChecks()
	{
		const std::filesystem::path path = CopyFox("checks");
		Model* model = path.empty() ? nullptr : Model::LoadFromFile(path);
		AnimationClip* walk = model ? model->FindAnimation("Walk") : nullptr;
		const AnimationClip* survey = model ? model->FindAnimation("Survey") : nullptr;
		const SubMesh* skinned = model ? FindSkinnedSubMesh(*model) : nullptr;
		if (!walk || !survey || !skinned || !skinned->DiffuseTexture)
		{
			Check(false, "a copy of the Fox loads for the reload checks");
			DestroyAndDelete(model);
			std::error_code error;
			if (!path.empty())
				std::filesystem::remove_all(path.parent_path(), error);
			return;
		}

		const std::string original = ReadText(path);
		const std::filesystem::path eventsPath = std::filesystem::path(path).replace_extension(".events");
		const Skeleton* skeleton = model->GetSkeleton();
		Mesh* mesh = skinned->MeshData;
		Texture* diffuse = skinned->DiffuseTexture;
		const uint64_t meshId = mesh->GetId();
		const uint64_t walkId = walk->GetId();
		const uint64_t skeletonId = skeleton->GetId();
		const uint32_t textureGeneration = diffuse->GetGeneration();

		Animator animator(skeleton);
		animator.Play(walk);
		animator.SetTime(0.25f);
		animator.Update(0.05f);
		const std::string_view heldName = animator.GetEventsThisFrame().empty() ? std::string_view() : animator.GetEventsThisFrame().front().Name;

		const bool reloaded = model->Reload();
		const SubMesh* after = FindSkinnedSubMesh(*model);
		Check(reloaded && model->GetGeneration() == 1 && model->FindAnimation("Walk") == walk && model->GetSkeleton() == skeleton
			&& skeleton->GetId() == skeletonId && after && after->MeshData == mesh && after->DiffuseTexture == diffuse,
			"Model::Reload keeps the model's clips, skeleton, meshes and textures where they were");
		const bool imageKept = diffuse->GetGeneration() == textureGeneration;
		const std::filesystem::path image = path.parent_path() / "Texture.png";
		WriteText(image, ReadText(image));
		const bool imageSaved = model->Reload();
		Check(mesh->GetId() != meshId && walk->GetId() != walkId && imageKept && imageSaved && diffuse->GetGeneration() != textureGeneration,
			"and gives the meshes and clips new ids, re-reading the texture in place only once its image is saved again");
		Check(heldName == "step_fl", "an event name read before a reload still reads the same after it");

		const AnimationClip* foxRun = m_Fox->FindAnimation("Run");
		const std::vector<JointPose> runPose = PoseAt(*m_Fox->GetSkeleton(), foxRun, 0.5f);
		animator.SetTime(0.5f);
		// Run, past the end of Walk's shorter loop, which Run takes.
		AnimationClip* run = model->FindAnimation("Run");
		Animator looping(skeleton);
		looping.Play(run);
		looping.SetTime(1.05f);
		looping.Update(0.0f);
		WriteText(path, SwapWalkAndRun(original));
		const bool swapped = model->Reload();
		animator.Update(0.0f);
		const float swapGap = PoseGap(animator.GetLocalPoses(), runPose);
		Check(swapped && animator.GetCurrentClip() == walk && walk->GetDuration() == foxRun->GetDuration() && swapGap < 1e-5f,
			std::format("a re-export that gives Walk Run's keys plays them through the same clip, in an animator that kept playing (gap {:.1e})", swapGap));

		looping.Update(0.05f);
		const float carriedOn = run ? std::fmod(1.05f, run->GetDuration()) + 0.05f : 0.0f;
		Check(run && run->GetDuration() < 1.05f && looping.GetEventsThisFrame().empty() && std::abs(looping.GetTime() - carriedOn) < 1e-4f,
			std::format("a reload that shortens a playing loop goes on from the same point of the new loop ({:.3f} s) instead of firing every mark from its start at once", looping.GetTime()));

		WriteText(eventsPath, "Walk 0.1 reload_mark\n");
		const bool eventsReloaded = model->Reload();
		animator.SetTime(0.0f);
		animator.Update(0.15f);
		Check(eventsReloaded && walk->GetEvents().size() == 1 && CountEvents(animator.GetEventsThisFrame(), "reload_mark", AnimationEventType::Instant) == 1,
			"the .events file reloads with its model: a new mark fires and the old ones are gone");

		Animator looking(skeleton);
		WriteText(eventsPath, "Survey 0.90..2.50 look\n");
		model->ReloadEvents();
		looking.Play(survey);
		looking.SetTime(0.85f);
		looking.Update(0.1f);
		const bool opened = looking.IsEventActive("look");
		const uint64_t meshBefore = mesh->GetId();
		const uint32_t generationBefore = model->GetGeneration();
		WriteText(eventsPath, "Survey 0.90..2.50 gaze\n");
		const bool renamedEvents = model->ReloadEvents();
		looking.Update(0.05f);
		const int lookEnds = CountEvents(looking.GetEventsThisFrame(), "look", AnimationEventType::RangeEnd);
		const bool lookClosed = !looking.IsEventActive("look");
		int gazeEvents = 0;
		for (int i = 0; i < 40; ++i)
		{
			looking.Update(0.05f);
			gazeEvents += CountEvents(looking.GetEventsThisFrame(), "gaze", AnimationEventType::RangeBegin) + CountEvents(looking.GetEventsThisFrame(), "gaze", AnimationEventType::RangeEnd);
		}
		Check(opened && renamedEvents && mesh->GetId() == meshBefore && model->GetGeneration() == generationBefore + 1,
			"ReloadEvents replaces the clips' events and leaves the meshes alone");
		Check(lookEnds == 1 && lookClosed && gazeEvents == 0,
			std::format("a range open when the .events file renames it ends with its RangeEnd at once, and the new name's end fires nothing without its begin ({} ends, {} gaze)", lookEnds, gazeEvents));

		DE_INFO("Animation Test: the next Model::LoadFromFile error is expected");
		WriteText(path, "{ not a glTF");
		const uint32_t generation = model->GetGeneration();
		const std::vector<JointPose> held(animator.GetLocalPoses().begin(), animator.GetLocalPoses().end());
		const bool broken = model->Reload();
		animator.Update(0.0f);
		Check(!broken && model->GetGeneration() == generation && walk->GetDuration() == foxRun->GetDuration() && PoseGap(animator.GetLocalPoses(), held) == 0.0f,
			"a file that no longer loads leaves the model as it was, and Reload returns false");

		// b_Tail02_013's rest offset, doubled.
		WriteText(path, Replace(original, "12.411918640136719", "24.823837280273438"));
		const int32_t tail = skeleton->FindJoint("b_Tail02_013");
		const uint32_t revision = skeleton->GetRevision();
		const std::vector<MeshVertex> restVertices = mesh->GetVertices();
		animator.Stop();
		const bool moved = model->Reload();
		animator.Update(0.0f);
		const float restGap = glm::length(animator.GetLocalPoses()[tail].Translation - glm::vec3(24.823837f, 0.0f, 0.0f));
		const bool verticesMoved = mesh->GetVertices().size() == restVertices.size() && !std::equal(restVertices.begin(), restVertices.end(), mesh->GetVertices().begin(),
			[](const MeshVertex& a, const MeshVertex& b) { return a.Position == b.Position; });
		Check(moved && model->GetSkeleton() == skeleton && skeleton->GetRevision() != revision && restGap < 1e-4f && verticesMoved,
			std::format("a moved joint keeps the skeleton and reaches an animator's rest pose and the mesh's rest vertices (off by {:.1e})", restGap));

		{
			WriteText(path, original);
			model->Reload();

			Scene scene("Reload checks");
			Entity fox = scene.CreateEntity("Fox");
			fox.AddComponent<Transform3DComponent>();
			fox.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(model));
			fox.AddComponent<AnimatorComponent>(AnimatorComponent("Walk"));
			scene.OnStart();
			scene.OnUpdate(0.05f);
			Animator* playing = scene.GetAnimator(fox);
			if (playing)
				playing->Play(survey);
			scene.OnUpdate(0.05f);

			const float before = playing ? playing->GetTime() : 0.0f;
			model->Reload();
			scene.OnUpdate(0.05f);
			Check(playing && scene.GetAnimator(fox) == playing && playing->GetCurrentClip() == survey && std::abs(playing->GetTime() - before - 0.05f) < 1e-5f,
				"a scene's animator keeps its clip and time through a reload that keeps the joints");

			WriteText(path, Replace(original, "\"name\": \"b_Tail03_014\"", "\"name\": \"b_Tail03_014x\""));
			const bool renamed = model->Reload();
			const std::vector<JointPose> resting(animator.GetLocalPoses().begin(), animator.GetLocalPoses().end());
			animator.Play(walk);
			animator.Update(0.1f);
			scene.OnUpdate(0.05f);
			Animator* restarted = scene.GetAnimator(fox);
			Check(renamed && model->GetSkeleton() != skeleton && animator.GetSkeleton() == skeleton && PoseGap(animator.GetLocalPoses(), resting) > 0.01f
				&& restarted && restarted->GetSkeleton() == model->GetSkeleton() && restarted->GetCurrentClip() == walk,
				"a renamed joint brings a new skeleton: an animator on the old one still runs, and a scene's starts over on the new one with its DefaultClip");
			scene.OnStop();
		}
		DestroyAndDelete(model);

		AssetManager& assets = Application::Get().GetAssetManager();
		WriteText(path, original);
		const AssetHandle handle = assets.Load(path);
		Model* managed = assets.GetModel(handle);
		const bool managedReload = managed && assets.Reload(handle);
		Check(AssetManager::SupportsInPlaceReload(AssetType::Model) && managedReload && assets.GetModel(handle) == managed && managed->GetGeneration() == 1,
			"AssetManager::Reload refreshes a model in place, so GetModel keeps handing out the same pointer");
		assets.Remove(handle);

		std::error_code error;
		std::filesystem::remove_all(path.parent_path(), error);
	}

	void AnimationTest::UpdateLiveReload()
	{
		if (m_FoxAsset == k_InvalidAsset || !m_Fox || m_ReloadStep >= 3)
			return;

		const std::filesystem::path& path = m_Fox->GetFilePath();
		const std::filesystem::path eventsPath = std::filesystem::path(path).replace_extension(".events");
		if (m_ReloadStep == 0)
		{
			if (m_Time < 1.0f)
				return;
			WriteText(path, SwapWalkAndRun(ReadText(path)));
		}
		else if (m_Fox->GetGeneration() == m_ReloadGeneration)
		{
			if (m_Time - m_ReloadStart < 6.0f)
				return;
			Check(false, m_ReloadStep == 1 ? "hot-reload picks up the edited Fox within 6 s" : "hot-reload picks up the edited Fox.events within 6 s");
			m_ReloadStep = 3;
			return;
		}
		else if (m_ReloadStep == 1)
		{
			const AnimationClip* walk = m_Fox->FindAnimation("Walk");
			const bool swapped = walk && walk->GetDuration() == m_RunDuration;
			const float seconds = m_Time - m_ReloadStart;
			if (m_RunPose.empty())
			{
				Check(swapped, std::format("hot-reload swapped the edited Fox in place {:.1f} s after the save", seconds));
			}
			else
			{
				// GetAnimator poses a paused animator for the reload at once.
				const Animator* animator = m_AnimatedFox ? m_Scene->GetAnimator(m_AnimatedFox) : nullptr;
				const float gap = animator ? PoseGap(animator->GetLocalPoses(), m_RunPose) : 1.0f;
				Check(swapped && gap < 1e-5f, std::format("hot-reload swapped the edited Fox in place {:.1f} s after the save, and the paused Fox shows Run's keys (gap {:.1e})", seconds, gap));
			}
			const SubMesh* skinned = FindSkinnedSubMesh(*m_Fox);
			m_ReloadMeshId = skinned ? skinned->MeshData->GetId() : 0;
			WriteText(eventsPath, ReadText(eventsPath) + "Walk 0.5 live_mark\n");
		}
		else
		{
			const AnimationClip* walk = m_Fox->FindAnimation("Walk");
			const SubMesh* skinned = FindSkinnedSubMesh(*m_Fox);
			const bool marked = walk && std::any_of(walk->GetEvents().begin(), walk->GetEvents().end(), [](const AnimationClipEvent& event) { return event.Name == "live_mark"; });
			Check(marked && skinned && skinned->MeshData->GetId() == m_ReloadMeshId,
				std::format("saving only Fox.events hot-reloads its events {:.1f} s later, with the new mark, and leaves the meshes alone", m_Time - m_ReloadStart));
		}

		m_ReloadGeneration = m_Fox->GetGeneration();
		m_ReloadStart = m_Time;
		++m_ReloadStep;
	}

	void AnimationTest::RecordEvent(Entity fox, const AnimationEvent& event)
	{
		m_EventLog.push_back({ event.Clip ? event.Clip->GetName() : std::string(), std::string(event.Name), event.Type, event.Time, m_Time });
		if (m_EventLog.size() > 16)
			m_EventLog.erase(m_EventLog.begin());

		const char* joint = event.Type == AnimationEventType::Instant ? StepJoint(event.Name) : nullptr;
		Animator* animator = joint ? m_Scene->GetAnimator(fox) : nullptr;
		const int32_t index = animator ? m_Fox->GetSkeleton()->FindJoint(joint) : Skeleton::k_InvalidJoint;
		if (index == Skeleton::k_InvalidJoint)
			return;

		glm::vec3 foot(fox.GetWorldTransform() * animator->GetJointTransform(index)[3]);
		foot.y = 0.003f;
		m_Footprints.push_back(foot);
		if (m_Footprints.size() > 48)
			m_Footprints.erase(m_Footprints.begin());
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

		{
			// Nothing asks for the animator before the bodies are built.
			Scene baked("Animation checks: bake");
			Entity walker = baked.CreateEntity("Fox");
			walker.AddComponent<Transform3DComponent>(Transform3DComponent({ 2.0f, 0.0f, -1.0f }, glm::vec3(m_FoxScale)));
			walker.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_Fox));
			walker.AddComponent<AnimatorComponent>(AnimatorComponent("Walk"));
			Entity guard = baked.CreateEntity("Guard");
			guard.AddComponent<Transform3DComponent>(Transform3DComponent(k_HatOffset, glm::vec3(k_HatSize)));
			guard.AddComponent<RigidBody3DComponent>().Type = BodyType3D::Kinematic;
			guard.SetParent(walker, k_HatJoint, false);
			baked.OnStart();

			Animator firstFrame(m_Fox->GetSkeleton());
			firstFrame.Play(walk);
			firstFrame.Evaluate();
			const glm::mat4 guardLocal = guard.GetComponent<Transform3DComponent>().GetTransform();
			const Skeleton& skeleton = *m_Fox->GetSkeleton();
			const glm::vec3 walking(walker.GetWorldTransform() * SocketFrame(firstFrame.GetJointTransform(head)) * guardLocal[3]);
			const glm::vec3 resting(walker.GetWorldTransform() * SocketFrame(skeleton.GetRootTransform() * skeleton.GetRestGlobalTransforms()[head]) * guardLocal[3]);
			Physics3D* physics = baked.GetPhysics3D();
			const glm::vec3 body = physics ? physics->GetPosition(baked.GetRuntimeBody3D(guard)) : glm::vec3(1e9f);
			const float gap = glm::length(body - walking);
			Check(gap < 1e-4f && glm::length(walking - resting) > 1e-3f,
				std::format("a body on a joint is built at DefaultClip's first frame, not the rest pose (off by {:.1e})", gap));
			baked.OnStop();
		}
		{
			Scene ranged("Animation checks: removal");
			Entity surveyor = ranged.CreateEntity("Fox");
			surveyor.AddComponent<Transform3DComponent>();
			surveyor.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_Fox));
			surveyor.AddComponent<AnimatorComponent>(AnimatorComponent("Survey"));
			int lookEnds = 0;
			surveyor.AddScript<EventListener>([&](Entity, const AnimationEvent& event) { lookEnds += event.Name == "look" && event.Type == AnimationEventType::RangeEnd; });
			ranged.OnStart();
			for (int frame = 0; frame < 60; ++frame)
				ranged.OnUpdate(1.0f / 60.0f);
			const Animator* looking = ranged.GetAnimator(surveyor);
			const bool open = looking && looking->IsEventActive("look");
			surveyor.RemoveComponent<AnimatorComponent>();
			ranged.OnUpdate(1.0f / 60.0f);
			Check(open && lookEnds == 1, "removing an AnimatorComponent mid-range sends the entity's script that range's RangeEnd");
			ranged.OnStop();
		}
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
			: m_Mode == Mode::Blend || m_Mode == Mode::Events ? std::string() : m_ClipName;
		const bool animate = m_Mode == Mode::Blend || m_Mode == Mode::Events || ((m_Mode == Mode::Clip || m_Mode == Mode::Layers || (m_Mode == Mode::Crowd && !m_CrowdStatic)) && !clipName.empty());
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
			// The overlay's bones draw first, in the batches, so a see-through skin shows them.
			const glm::vec4 skin(1.0f, 1.0f, 1.0f, SkeletonShown() ? 0.35f : 1.0f);
			if (drawStatic)
				fox.AddComponent<MeshRendererComponent>(MeshRendererComponent(skinned->MeshData, skin)).Material = m_FoxMaterial;
			else
				fox.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_Fox, skin)).Material = m_FoxMaterial;
			if (m_Mode != Mode::Crowd)
				m_Foxes.push_back(fox);

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
			if (m_Mode == Mode::Blend || m_Mode == Mode::Events)
			{
				animator->SetFloat(k_SpeedParameter, m_BlendSpeed);
				animator->Play(AnimationState::Blend1D(k_SpeedParameter,
					{ { 0.0f, m_Fox->FindAnimation("Survey") }, { 1.5f, m_Fox->FindAnimation("Walk") }, { 4.0f, m_Fox->FindAnimation("Run") } }));
				if (m_Mode == Mode::Events)
					animated[i].AddScript<EventListener>([this](Entity fox, const AnimationEvent& event) { RecordEvent(fox, event); });
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
		m_Foxes.clear();
		delete m_Scene;
		m_Scene = nullptr;
	}

	void AnimationTest::SubmitSkeleton(Renderer3D& renderer)
	{
		m_SkeletonBoxes = 0;
		const Skeleton& skeleton = *m_Fox->GetSkeleton();
		const float thickness = 0.012f;
		std::vector<glm::vec3> joints(skeleton.GetJointCount());
		for (Entity fox : m_Foxes)
		{
			const Animator* animator = m_Scene->GetAnimator(fox);
			const glm::mat4 world = fox.GetWorldTransform();
			for (uint32_t i = 0; i < skeleton.GetJointCount(); ++i)
			{
				const glm::mat4 joint = animator ? animator->GetJointTransform(static_cast<int32_t>(i)) : skeleton.GetRootTransform() * skeleton.GetRestGlobalTransforms()[i];
				joints[i] = glm::vec3(world * joint[3]);
			}

			for (uint32_t i = 0; i < skeleton.GetJointCount(); ++i)
			{
				const int32_t parent = skeleton.GetJoint(i).Parent;
				if (parent >= 0)
				{
					renderer.SubmitMesh(renderer.GetBoxMesh(), BoneBox(joints[parent], joints[i], thickness), { 0.95f, 0.9f, 0.3f, 1.0f });
					++m_SkeletonBoxes;
				}
				renderer.SubmitMesh(renderer.GetBoxMesh(), glm::translate(glm::mat4(1.0f), joints[i]) * glm::scale(glm::mat4(1.0f), glm::vec3(2.5f * thickness)), { 1.0f, 0.4f, 0.2f, 1.0f });
				++m_SkeletonBoxes;
			}
		}
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
			case Mode::Events:
				Check(stats.SkinnedDraws == 1 && stats.SkinnedJoints == 24 && stats.DroppedSkinnedDraws == 0, "the stepping Fox is one skinned draw of 24 joints");
				break;
			case Mode::Clip:
				Check(stats.SkinnedDraws == 1 && stats.SkinnedInstances == 1 && stats.SkinnedJoints == 24 && stats.DroppedSkinnedDraws == 0, "the animated Fox is one skinned draw of 24 joints");
				if (SkeletonShown())
				{
					// The floor, the hat and the Fox, then a box per joint and per bone.
					const Skeleton& skeleton = *m_Fox->GetSkeleton();
					const uint32_t bones = static_cast<uint32_t>(std::count_if(skeleton.GetJoints().begin(), skeleton.GetJoints().end(), [](const Joint& joint) { return joint.Parent >= 0; }));
					Check(m_SkeletonBoxes == skeleton.GetJointCount() + bones && stats.SubmittedMeshes == 3 + m_SkeletonBoxes,
						std::format("the skeleton overlay draws the Fox's {} joints and {} bones", skeleton.GetJointCount(), bones));
				}
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

	void AnimationTest::RunSkinnedMaterialCheck(Renderer3D& renderer)
	{
		m_SkinnedMaterialChecked = true;
		const SubMesh* skinned = m_Fox ? FindSkinnedSubMesh(*m_Fox) : nullptr;
		m_GhostShader = Shader::Create(ShaderParams().SetName("AnimationTestGhost").SetSourceCode(k_GhostShader).AddDefine("DE_SKINNED"));
		if (!skinned || !m_GhostShader || !m_GhostShader->IsValid())
		{
			Check(false, "the custom skinned shader compiles");
			return;
		}
		m_Ghost = Material::Create(MaterialParams().SetShader(m_GhostShader).SetCullMode(CullMode::None).SetDebugName("Ghost"));

		// Its own scene, which the frame's scene clears away.
		renderer.BeginScene(m_Camera);
		renderer.SubmitMesh(renderer.GetBoxMesh(), glm::mat4(1.0f), glm::vec4(1.0f), m_Ghost);
		renderer.SubmitSkinnedMesh(skinned->MeshData, glm::mat4(1.0f), m_Fox->GetSkeleton()->GetRestPalette(), glm::vec4(1.0f), m_Ghost);
		renderer.EndScene();

		const Renderer3D::Statistics& stats = renderer.GetStatistics();
		Check(stats.SkinnedDraws == 1 && stats.SubmittedMeshes == 2 && stats.DrawCalls == 2,
			"a custom skinned material skins the Fox, and a static box given it draws with the default material instead of a pipeline its vertex stage can't take");
	}

	void AnimationTest::Update(float deltaTime)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		if (!m_SkinnedMaterialChecked && m_Scene)
			RunSkinnedMaterialCheck(renderer);

		renderer.BeginScene(m_Camera);
		renderer.Clear(m_ClearColor);
		if (!m_Scene)
		{
			renderer.EndScene();
			return;
		}

		if (Animator* animator = (m_Mode == Mode::Blend || m_Mode == Mode::Layers || m_Mode == Mode::Events) && m_AnimatedFox ? m_Scene->GetAnimator(m_AnimatedFox) : nullptr)
		{
			animator->SetFloat(k_SpeedParameter, m_BlendSpeed);
			if (m_Mode == Mode::Layers)
				animator->SetLayerWeight(1, m_LayerWeight);
			// A frozen Fox isn't advanced, so the sliders still show at once.
			if (!m_AnimatedFox.GetComponent<AnimatorComponent>().Enabled)
				animator->Update(0.0f);
		}

		UpdateLiveReload();

		const Clock::time_point updateStart = Clock::now();
		m_Scene->OnUpdate((std::min)(deltaTime, 1.0f / 30.0f));
		const Clock::time_point updateEnd = Clock::now();
		m_Scene->SubmitLights(renderer);

		const Clock::time_point renderStart = Clock::now();
		m_Scene->RenderEntities3D(renderer);
		if (m_Mode == Mode::Pose)
			SubmitPosedFox(renderer);
		if (SkeletonShown())
			SubmitSkeleton(renderer);
		for (const glm::vec3& footprint : m_Footprints)
			renderer.SubmitMesh(renderer.GetBoxMesh(), glm::translate(glm::mat4(1.0f), footprint) * glm::scale(glm::mat4(1.0f), glm::vec3(0.05f, 0.004f, 0.07f)), { 0.12f, 0.09f, 0.07f, 1.0f });
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
		DestroyAndDelete(m_Ghost);
		DestroyAndDelete(m_GhostShader);
		if (m_FoxAsset != k_InvalidAsset)
		{
			const std::filesystem::path folder = m_Fox ? m_Fox->GetFilePath().parent_path() : std::filesystem::path();
			AssetManager& assets = Application::Get().GetAssetManager();
			assets.Remove(m_FoxAsset);
			assets.SetHotReloadEnabled(m_HotReloadWas);
			m_FoxAsset = k_InvalidAsset;
			m_Fox = nullptr;

			std::error_code error;
			if (!folder.empty())
				std::filesystem::remove_all(folder, error);
		}
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
		ImGui::RadioButton("Events", &mode, static_cast<int>(Mode::Events));
		ImGui::SameLine();
		ImGui::RadioButton("Crowd", &mode, static_cast<int>(Mode::Crowd));
		if (mode != static_cast<int>(m_Mode))
		{
			m_Mode = static_cast<Mode>(mode);
			Cleanup();
			Initialize();
			return;
		}

		if (m_Mode != Mode::Crowd && m_Mode != Mode::Pose && ImGui::Checkbox("Skeleton", &m_ShowSkeleton))
		{
			Cleanup();
			Initialize();
			return;
		}
		ImGui::Text("Skinned draws %u, instances %u, dropped %u, joints %u, draw calls %u",
			m_LastStats.SkinnedDraws, m_LastStats.SkinnedInstances, m_LastStats.DroppedSkinnedDraws, m_LastStats.SkinnedJoints, m_LastStats.DrawCalls);
		if (m_Mode == Mode::Crowd)
			ImGui::TextWrapped("Timing: %s", m_TimingResult.empty() ? "measuring..." : m_TimingResult.c_str());
		if (m_Mode == Mode::Blend || m_Mode == Mode::Events)
			ImGui::SliderFloat("Speed (Survey 0, Walk 1.5, Run 4)", &m_BlendSpeed, 0.0f, 4.0f);
		if (m_Mode == Mode::Events)
		{
			ImGui::Text("Event log (newest last)");
			for (const LoggedEvent& event : m_EventLog)
			{
				const char* type = event.Type == AnimationEventType::RangeBegin ? "begin" : event.Type == AnimationEventType::RangeEnd ? "end" : "";
				ImGui::Text("%7.2f s  %-6s %-8s %s @ %.3f", event.At, event.Clip.c_str(), event.Name.c_str(), type, event.Time);
			}
		}
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
