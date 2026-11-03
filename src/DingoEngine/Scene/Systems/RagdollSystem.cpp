#include "depch.h"
#include "DingoEngine/Scene/Systems/RagdollSystem.h"

#include "DingoEngine/Graphics/Animator.h"
#include "DingoEngine/Graphics/Model.h"
#include "DingoEngine/Graphics/Skeleton.h"
#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Systems/AnimationSystem.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"
#include "DingoEngine/Scene/Systems/RuntimeComponents.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>

namespace Dingo
{

	namespace Internal
	{

		namespace
		{

			enum class JointClass
			{
				Hinge,
				Torso,
				Arm,
				Leg,
				Other,
			};

			struct SwingTwistLimits
			{
				float SwingY;
				float SwingZ;
				float Twist;
			};

			// A motor's stiffness on its own body alone stays below this fraction of the step rate. A light
			// body holding a heavy subtree (the Fox's tail root) otherwise rings near the step rate under
			// the subtree's gains, and the iterative solver feeds energy into it until the pose swings.
			constexpr float k_MaxBodyFrequencyPerStep = 0.25f;
			constexpr float k_MaxDriveFrequency = 1000.0f;
			constexpr float k_MaxTargetStep = 0.5f;
			constexpr float k_MaxTargetTurn = 1.0f;
			constexpr float k_GroundProbe = 100.0f;
			constexpr float k_SeatSpeed = 2.0f;
			constexpr float k_SoftStrength = 0.25f;
			constexpr float k_SoftSeconds = 1.0f;
			constexpr float k_FoldRatio = 0.3f;
			constexpr float k_HingeMaxBend = glm::radians(150.0f);
			constexpr float k_HingeOverextension = glm::radians(5.0f);

			std::string Lowercase(std::string_view text)
			{
				std::string lower(text);
				std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				return lower;
			}

			bool ContainsAny(const std::string& name, std::initializer_list<std::string_view> words)
			{
				return std::any_of(words.begin(), words.end(), [&](std::string_view word) { return name.find(word) != std::string::npos; });
			}

			// Fingers, faces, twist and IK helpers and attachment points never get a body of their own.
			bool IsFoldedByName(const std::string& name)
			{
				return ContainsAny(name, { "finger", "thumb", "index", "middle", "ring", "pinky", "toe", "eye", "jaw", "tongue", "twist", "slot", "socket", "ik_", "_ik", "pole" });
			}

			bool EndsWith(const std::string& name, std::string_view suffix)
			{
				return name.size() >= suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
			}

			JointClass Classify(const std::string& name)
			{
				if (ContainsAny(name, { "forearm", "lowerarm", "lower_arm", "elbow", "knee", "calf", "shin", "lowerleg", "lower_leg" }))
					return JointClass::Hinge;
				// Mixamo's knee is "LeftLeg", below "LeftUpLeg".
				if (EndsWith(name, "leg") && !ContainsAny(name, { "up", "thigh" }))
					return JointClass::Hinge;
				if (ContainsAny(name, { "arm", "shoulder", "clavicle", "hand", "wrist" }))
					return JointClass::Arm;
				if (ContainsAny(name, { "leg", "thigh", "foot", "ankle" }))
					return JointClass::Leg;
				if (ContainsAny(name, { "hip", "pelvis", "spine", "chest", "neck", "head" }))
					return JointClass::Torso;
				return JointClass::Other;
			}

			SwingTwistLimits LimitsOf(JointClass type)
			{
				switch (type)
				{
					case JointClass::Torso: return { glm::radians(30.0f), glm::radians(30.0f), glm::radians(20.0f) };
					case JointClass::Arm:   return { glm::radians(70.0f), glm::radians(70.0f), glm::radians(45.0f) };
					case JointClass::Leg:   return { glm::radians(60.0f), glm::radians(45.0f), glm::radians(30.0f) };
					default:                return { glm::radians(25.0f), glm::radians(25.0f), glm::radians(15.0f) };
				}
			}

			glm::quat FromTo(const glm::vec3& from, const glm::vec3& to)
			{
				const float d = glm::dot(from, to);
				if (d < -0.9999f)
				{
					glm::vec3 axis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), from);
					if (glm::dot(axis, axis) < 1e-6f)
						axis = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), from);
					return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
				}
				const glm::vec3 c = glm::cross(from, to);
				return glm::normalize(glm::quat(1.0f + d, c.x, c.y, c.z));
			}

			// `direction` with its component along `axis` removed, normalised; `fallback` (likewise) when
			// that leaves nothing.
			glm::vec3 Perpendicular(const glm::vec3& direction, const glm::vec3& axis, const glm::vec3& fallback)
			{
				glm::vec3 result = direction - glm::dot(direction, axis) * axis;
				if (glm::dot(result, result) < 1e-6f)
					result = fallback - glm::dot(fallback, axis) * axis;
				if (glm::dot(result, result) < 1e-6f)
					result = glm::cross(axis, std::abs(axis.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f));
				return glm::normalize(result);
			}

			const Model* ModelOf(const entt::registry& registry, entt::entity handle)
			{
				const SkinnedMeshRendererComponent* skinned = registry.try_get<SkinnedMeshRendererComponent>(handle);
				return skinned ? skinned->Model : nullptr;
			}

			const Skeleton* SkeletonOf(const entt::registry& registry, entt::entity handle)
			{
				const Model* model = ModelOf(registry, handle);
				return model ? model->GetSkeleton() : nullptr;
			}

			uint32_t RecipeRevisionOf(const entt::registry& registry, entt::entity handle)
			{
				const Model* model = ModelOf(registry, handle);
				return model ? model->GetRagdollRecipeRevision() : 0;
			}

			float CapsuleVolume(float radius, float halfHeight)
			{
				return glm::pi<float>() * radius * radius * (2.0f * halfHeight + 4.0f / 3.0f * radius);
			}

			// Per part, the 85th-percentile distance from its bone of the rest-pose vertices whose heaviest joint
			// rides on it, as a fraction of the bone's rest length; 0 where too few vertices say.
			std::vector<float> MeasureRadii(const Model& model, const Skeleton& skeleton, const RagdollRuntime& runtime,
				const std::vector<int32_t>& mainChild, const std::vector<glm::vec3>& restPositions)
			{
				constexpr size_t k_MinSamples = 8;
				constexpr float k_RadiusPercentile = 0.85f;
				std::vector<std::vector<float>> distances(runtime.Parts.size());
				const uint32_t jointCount = skeleton.GetJointCount();
				for (const SubMesh& submesh : model.GetSubMeshes())
				{
					if (!submesh.Mesh)
						continue;
					const std::vector<MeshVertex>& vertices = submesh.Mesh->GetVertices();
					const std::vector<SkinnedMeshVertex>& skin = submesh.Mesh->GetSkinVertices();
					if (skin.size() != vertices.size())
						continue;

					for (size_t v = 0; v < skin.size(); ++v)
					{
						int heaviest = 0;
						for (int k = 1; k < 4; ++k)
						{
							if (skin[v].Weights[k] > skin[v].Weights[heaviest])
								heaviest = k;
						}
						const uint32_t joint = skin[v].Joints[heaviest];
						if (joint >= jointCount || runtime.PartOfJoint[joint] < 0)
							continue;

						const int32_t p = runtime.PartOfJoint[joint];
						const int32_t boneJoint = runtime.Parts[p].Joint;
						const glm::vec3 from = restPositions[boneJoint];
						const glm::vec3 bone = restPositions[mainChild[boneJoint]] - from;
						const float lengthSquared = glm::dot(bone, bone);
						if (lengthSquared <= 0.0f)
							continue;
						const glm::vec3 offset = vertices[v].Position - from;
						const float t = glm::dot(offset, bone) / lengthSquared;
						if (t < 0.0f || t > 1.0f)
							continue;
						distances[p].push_back(glm::length(offset - t * bone) / (std::sqrt)(lengthSquared));
					}
				}

				std::vector<float> radii(runtime.Parts.size(), 0.0f);
				for (size_t p = 0; p < distances.size(); ++p)
				{
					std::vector<float>& samples = distances[p];
					if (samples.size() < k_MinSamples)
						continue;
					auto middle = samples.begin() + static_cast<std::ptrdiff_t>(static_cast<float>(samples.size() - 1) * k_RadiusPercentile);
					std::nth_element(samples.begin(), middle, samples.end());
					radii[p] = *middle;
				}
				return radii;
			}

			std::string NameOf(const entt::registry& registry, entt::entity handle)
			{
				const TagComponent* tag = registry.try_get<TagComponent>(handle);
				return tag ? tag->Tag : std::string();
			}

			void RigidFrame(const glm::mat4& matrix, glm::vec3& position, glm::quat& rotation)
			{
				glm::vec3 scale;
				HierarchySystem::Decompose(matrix, position, rotation, scale);
			}

			bool Driven(const RagdollComponent& settings)
			{
				return settings.DriveToAnimation && settings.Strength > 0.0f && settings.DriveFrequency > 0.0f;
			}

			bool GroundUnderRoot(const Physics3D& physics, const RagdollRuntime& runtime, float& groundY)
			{
				QueryFilter3D filter;
				for (const RagdollRuntime::Part& part : runtime.Parts)
					filter.Ignore(part.Body);
				RayCastHit3D hit;
				if (!physics.RayCast(Ray(physics.GetPosition(runtime.Parts.front().Body), glm::vec3(0.0f, -1.0f, 0.0f)), k_GroundProbe, hit, filter))
					return false;
				groundY = hit.Point.y;
				return true;
			}

			bool Finite(const glm::vec3& v)
			{
				return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
			}

		}

		Animator* RagdollSystem::PosingAnimator(entt::registry& registry, entt::entity handle, const Skeleton& skeleton)
		{
			Animator* animator = nullptr;
			if (registry.all_of<AnimatorComponent>(handle))
				animator = AnimationSystem::GetAnimator(registry, handle);
			else if (RagdollRuntime* runtime = registry.try_get<RagdollRuntime>(handle))
				animator = runtime->Instance.get();

			if (!animator || !animator->GetSkeleton() || animator->GetSkeleton()->GetId() != skeleton.GetId())
				return nullptr;
			if (animator->GetLocalPoses().size() < skeleton.GetJointCount())
				return nullptr;
			return animator;
		}

		bool RagdollSystem::AnimatedJointWorlds(entt::registry& registry, entt::entity handle, const Skeleton& skeleton, glm::mat4& entityWorld)
		{
			const Animator* animator = PosingAnimator(registry, handle, skeleton);
			if (!animator)
				return false;

			const size_t count = skeleton.GetJointCount();
			m_Globals.resize(count);
			m_JointWorlds.resize(count);
			skeleton.ComputeGlobalTransforms(animator->GetLocalPoses(), m_Globals);

			entityWorld = HierarchySystem::WorldTransform(registry, handle);
			const glm::mat4 model = entityWorld * skeleton.GetRootTransform();
			for (size_t i = 0; i < count; ++i)
				m_JointWorlds[i] = model * m_Globals[i];
			return true;
		}

		void RagdollSystem::Sync(entt::registry& registry, Physics3D& physics, float deltaTime)
		{
			m_Scratch.clear();
			auto built = registry.view<RagdollRuntime>();
			for (entt::entity handle : built)
			{
				const RagdollRuntime& runtime = built.get<RagdollRuntime>(handle);
				const Skeleton* skeleton = SkeletonOf(registry, handle);
				bool stale = !skeleton || skeleton->GetId() != runtime.SkeletonId || skeleton->GetRevision() != runtime.SkeletonRevision;
				const RagdollComponent* settings = registry.try_get<RagdollComponent>(handle);
				stale = stale || !settings || settings->Recipe != runtime.Recipe || RecipeRevisionOf(registry, handle) != runtime.RecipeRevision;
				// An AnimatorComponent added or removed after the build changes which animator poses it.
				stale = stale || registry.all_of<AnimatorComponent>(handle) == (runtime.Instance != nullptr);
				for (const RagdollRuntime::Part& part : runtime.Parts)
					stale = stale || !physics.IsBodyValid(part.Body) || (part.Parent >= 0 && !physics.IsConstraintValid(part.Constraint));
				if (stale)
					m_Scratch.push_back(handle);
			}
			for (entt::entity handle : m_Scratch)
				Drop(registry, &physics, handle);

			m_Scratch.clear();
			auto pending = registry.view<RagdollComponent, SkinnedMeshRendererComponent, Transform3DComponent>(entt::exclude<RagdollRuntime, InactiveComponent>);
			for (entt::entity handle : pending)
				m_Scratch.push_back(handle);
			for (entt::entity handle : m_Scratch)
				Build(registry, physics, handle);

			if (deltaTime <= 0.0f)
				return;

			m_Scratch.clear();
			for (entt::entity handle : registry.view<RagdollRuntime>())
				m_Scratch.push_back(handle);
			for (entt::entity handle : m_Scratch)
				Drive(registry, physics, handle, deltaTime);
		}

		void RagdollSystem::Build(entt::registry& registry, Physics3D& physics, entt::entity handle)
		{
			const Model* model = ModelOf(registry, handle);
			const Skeleton* skeleton = SkeletonOf(registry, handle);
			if (!model || !skeleton || skeleton->GetJointCount() == 0)
				return;

			const RagdollComponent& settings = registry.get<RagdollComponent>(handle);
			const uint32_t recipeRevision = model->GetRagdollRecipeRevision();
			if (const auto rejected = m_Rejected.find(handle); rejected != m_Rejected.end())
			{
				const Rejection& previous = rejected->second;
				if (previous.SkeletonId == skeleton->GetId() && previous.Revision == skeleton->GetRevision()
					&& previous.Recipe == settings.Recipe && previous.RecipeRevision == recipeRevision)
					return;
				m_Rejected.erase(rejected);
			}

			auto reject = [&](const char* reason)
			{
				m_Rejected[handle] = { skeleton->GetId(), skeleton->GetRevision(), settings.Recipe, recipeRevision };
				DE_CORE_ERROR("RagdollComponent on '{}': {}; no ragdoll built", NameOf(registry, handle), reason);
			};

			// The rest-pose animator joins the registry first so PosingAnimator finds it.
			RagdollRuntime& runtime = registry.emplace<RagdollRuntime>(handle);
			runtime.SkeletonId = skeleton->GetId();
			runtime.SkeletonRevision = skeleton->GetRevision();
			runtime.Recipe = settings.Recipe;
			runtime.RecipeRevision = recipeRevision;
			if (!registry.all_of<AnimatorComponent>(handle))
				runtime.Instance = std::make_unique<Animator>(skeleton);

			glm::mat4 entityWorld;
			if (!AnimatedJointWorlds(registry, handle, *skeleton, entityWorld))
			{
				registry.remove<RagdollRuntime>(handle);
				return;
			}

			const RagdollRecipe* recipe = model->FindRagdollRecipe(settings.Recipe);
			if (!recipe && !settings.Recipe.empty())
				DE_CORE_WARN("RagdollComponent on '{}': the model has no ragdoll recipe '{}', so it builds without one", NameOf(registry, handle), settings.Recipe);

			const std::vector<Joint>& joints = skeleton->GetJoints();
			const int32_t count = static_cast<int32_t>(joints.size());

			std::vector<glm::vec3> positions(count);
			std::vector<glm::quat> rotations(count);
			std::vector<std::string> names(count);
			std::vector<bool> excluded(count, false);
			std::vector<std::vector<int32_t>> children(count);
			std::vector<int32_t> descendants(count, 0);
			for (int32_t i = 0; i < count; ++i)
			{
				RigidFrame(m_JointWorlds[i], positions[i], rotations[i]);
				names[i] = Lowercase(joints[i].Name);
				const int32_t parent = joints[i].Parent;
				excluded[i] = IsFoldedByName(names[i]) || (recipe && recipe->IsExcluded(joints[i].Name)) || (parent >= 0 && excluded[parent]);
				if (parent >= 0)
					children[parent].push_back(i);
			}
			for (int32_t i = count - 1; i >= 0; --i)
			{
				if (joints[i].Parent >= 0)
					descendants[joints[i].Parent] += descendants[i] + 1;
			}

			auto mainChild = [&](int32_t joint)
			{
				int32_t best = -1;
				for (const int32_t child : children[joint])
				{
					if (best < 0 || (excluded[best] && !excluded[child]) || (excluded[best] == excluded[child] && descendants[child] > descendants[best]))
						best = child;
				}
				return best;
			};

			// The hips by name when they branch, so a root bone fanning out to IK targets or props doesn't
			// become a floor-to-pelvis body.
			int32_t root = -1;
			int32_t namedRoot = -1;
			for (int32_t i = 0; i < count && namedRoot < 0; ++i)
			{
				if (excluded[i])
					continue;
				const auto kept = std::count_if(children[i].begin(), children[i].end(), [&](int32_t child) { return !excluded[child]; });
				if (kept < 2)
					continue;
				if (root < 0)
					root = i;
				if (ContainsAny(names[i], { "hip", "pelvis" }))
					namedRoot = i;
			}
			if (namedRoot >= 0)
				root = namedRoot;
			if (root < 0)
			{
				registry.remove<RagdollRuntime>(handle);
				reject("the skeleton has no joint with two or more children to root it at");
				return;
			}

			std::vector<float> lengths(count, 0.0f);
			std::vector<int32_t> main(count, -1);
			for (int32_t i = 0; i < count; ++i)
			{
				main[i] = mainChild(i);
				if (main[i] >= 0)
					lengths[i] = glm::length(positions[main[i]] - positions[i]);
			}

			const float scale = glm::length(glm::vec3(entityWorld[1])) * glm::length(glm::vec3(skeleton->GetRootTransform()[1]));
			const float minLength = 1e-4f * (scale > 0.0f && std::isfinite(scale) ? scale : 1.0f);

			runtime.PartOfJoint.assign(count, -1);
			std::vector<bool> inRagdoll(count, false);
			for (int32_t i = root; i < count; ++i)
			{
				const int32_t parent = joints[i].Parent;
				if (i != root && (parent < 0 || !inRagdoll[parent]))
					continue;
				inRagdoll[i] = true;

				const int32_t ancestor = i == root ? -1 : runtime.PartOfJoint[parent];
				bool body = main[i] >= 0 && !excluded[i] && lengths[i] > minLength;
				if (body && ancestor >= 0)
					body = lengths[i] >= k_FoldRatio * lengths[runtime.Parts[ancestor].Joint];
				if (!body)
				{
					if (i == root)
					{
						registry.remove<RagdollRuntime>(handle);
						reject("the root joint's bone has no length");
						return;
					}
					runtime.PartOfJoint[i] = ancestor;
					continue;
				}

				if (runtime.Parts.size() >= k_MaxBodies)
				{
					registry.remove<RagdollRuntime>(handle);
					reject("the skeleton needs more than 20 bodies");
					return;
				}

				RagdollRuntime::Part& part = runtime.Parts.emplace_back();
				part.Joint = i;
				part.Parent = ancestor;
				runtime.PartOfJoint[i] = static_cast<int32_t>(runtime.Parts.size() - 1);
			}

			std::vector<const RagdollJointRecipe*> entries(runtime.Parts.size(), nullptr);
			if (recipe)
			{
				for (size_t p = 0; p < runtime.Parts.size(); ++p)
					entries[p] = recipe->FindJoint(joints[runtime.Parts[p].Joint].Name);
				for (const RagdollJointRecipe& entry : recipe->Joints)
				{
					const int32_t joint = skeleton->FindJoint(entry.Joint);
					const int32_t part = joint >= 0 ? runtime.PartOfJoint[joint] : -1;
					if (part < 0 || runtime.Parts[part].Joint != joint)
						DE_CORE_WARN("RagdollComponent on '{}': joint '{}' gets no body, so its recipe line does nothing", NameOf(registry, handle), entry.Joint);
				}
			}

			// Model space (the rest vertices' space) per joint, for radii given in or measured from it.
			std::vector<glm::vec3> restPositions(count);
			const std::vector<glm::mat4>& restGlobals = skeleton->GetRestGlobalTransforms();
			for (int32_t i = 0; i < count && i < static_cast<int32_t>(restGlobals.size()); ++i)
				restPositions[i] = glm::vec3(skeleton->GetRootTransform() * restGlobals[i][3]);
			const std::vector<float> measured = MeasureRadii(*model, *skeleton, runtime, main, restPositions);
			const float entityScale = glm::length(glm::vec3(entityWorld[1]));

			// Each bone's collider, sized by its recipe, else by the vertices it moves, else (automatic) by
			// its length and how far its other children sit off it.
			struct Shape
			{
				glm::vec3 Position;
				glm::quat Rotation;
				RagdollShape Type;
				float Radius;
				float HalfHeight;
				float Volume;
			};
			std::vector<Shape> shapes(runtime.Parts.size());
			float totalVolume = 0.0f;
			for (size_t p = 0; p < runtime.Parts.size(); ++p)
			{
				const int32_t joint = runtime.Parts[p].Joint;
				const float length = lengths[joint];
				const glm::vec3 direction = (positions[main[joint]] - positions[joint]) / length;
				const float restLength = glm::length(restPositions[main[joint]] - restPositions[joint]);
				// Model space to world along this bone, which also absorbs a pose that stretches it.
				const float toWorld = restLength > 1e-6f ? length / restLength : entityScale;

				float lateral = 0.0f;
				for (const int32_t child : children[joint])
				{
					const glm::vec3 offset = positions[child] - positions[joint];
					lateral = (std::max)(lateral, glm::length(offset - glm::dot(offset, direction) * direction));
				}
				const float automatic = (std::max)(0.22f * length, (std::min)(0.5f * lateral, 0.6f * length));

				Shape& shape = shapes[p];
				shape.Type = entries[p] ? entries[p]->Shape : RagdollShape::Capsule;
				if (entries[p] && entries[p]->Radius > 0.0f)
					shape.Radius = entries[p]->Radius * toWorld;
				else if (measured[p] > 0.0f)
					shape.Radius = std::clamp(measured[p] * length, 0.08f * length, 0.6f * length);
				else
					shape.Radius = automatic;
				if (!std::isfinite(shape.Radius) || shape.Radius <= 0.0f)
					shape.Radius = automatic;

				shape.HalfHeight = shape.Type == RagdollShape::Box ? 0.5f * length : (std::max)(0.5f * length - shape.Radius, 0.05f * length);
				shape.Position = positions[joint] + direction * (0.5f * length);
				shape.Rotation = FromTo(rotations[joint] * glm::vec3(0.0f, 1.0f, 0.0f), direction) * rotations[joint];
				// Mass follows the bone's size, not its collider's: a skin's fur and silhouette would weigh
				// a fluffy tail like a torso, and joints between such unequal bodies won't hold a pose.
				shape.Volume = CapsuleVolume(automatic, (std::max)(0.5f * length - automatic, 0.05f * length));
				totalVolume += shape.Volume;
			}

			const float totalMass = settings.TotalMass > 0.0f && std::isfinite(settings.TotalMass) ? settings.TotalMass : 70.0f;

			// A recipe's mass shares are exact while they leave room; the other parts split the rest by
			// volume. Shares that fill the total are scaled down with the others' volume shares.
			std::vector<float> fractions(runtime.Parts.size());
			float listed = 0.0f;
			float unlistedVolume = 0.0f;
			for (size_t p = 0; p < runtime.Parts.size(); ++p)
			{
				if (entries[p] && entries[p]->MassShare > 0.0f)
					listed += entries[p]->MassShare;
				else
					unlistedVolume += shapes[p].Volume;
			}
			const bool room = unlistedVolume <= 0.0f ? listed > 0.0f : listed < 1.0f;
			if (!room && unlistedVolume > 0.0f)
				DE_CORE_WARN("RagdollComponent on '{}': the recipe's mass shares add up to {:.2f}, leaving nothing for the other bones; all shares are scaled to fit", NameOf(registry, handle), listed);
			float sum = 0.0f;
			for (size_t p = 0; p < runtime.Parts.size(); ++p)
			{
				const bool share = entries[p] && entries[p]->MassShare > 0.0f;
				if (unlistedVolume <= 0.0f)
					fractions[p] = share ? entries[p]->MassShare : 0.0f;
				else if (room)
					fractions[p] = share ? entries[p]->MassShare : (1.0f - listed) * shapes[p].Volume / unlistedVolume;
				else
					fractions[p] = share ? entries[p]->MassShare : shapes[p].Volume / totalVolume;
				sum += fractions[p];
			}
			if (!(sum > 0.0f))
			{
				std::fill(fractions.begin(), fractions.end(), 1.0f);
				sum = static_cast<float>(fractions.size());
			}

			runtime.Group = physics.AllocateCollisionGroup();

			auto fail = [&](const char* reason)
			{
				for (const RagdollRuntime::Part& part : runtime.Parts)
				{
					physics.DestroyConstraint(part.Constraint);
					physics.DestroyBody(part.Body);
				}
				registry.remove<RagdollRuntime>(handle);
				reject(reason);
			};

			for (size_t p = 0; p < runtime.Parts.size(); ++p)
			{
				RagdollRuntime::Part& part = runtime.Parts[p];
				const Shape& shape = shapes[p];
				part.Mass = totalMass * fractions[p] / sum;

				const ColliderShape3D collider = shape.Type == RagdollShape::Sphere ? ColliderShape3D::Sphere
					: shape.Type == RagdollShape::Box ? ColliderShape3D::Box : ColliderShape3D::Capsule;
				RigidBodyParams3D params(BodyType3D::Dynamic, collider);
				params.Position = shape.Position;
				params.Rotation = shape.Rotation;
				params.Radius = shape.Radius;
				params.HalfHeight = shape.HalfHeight;
				params.HalfExtents = glm::vec3(shape.Radius, shape.HalfHeight, shape.Radius);
				params.Mass = part.Mass;
				params.Friction = 0.8f;
				params.CollisionGroup = runtime.Group;
				params.CollideWithinGroup = settings.CollideWithinRagdoll;
				params.UserData = static_cast<std::uint32_t>(handle);
				part.Body = physics.CreateBody(params);
				if (part.Body == k_InvalidBody3D)
				{
					fail("a bone's capsule could not be created");
					return;
				}

				const glm::quat inverseJoint = glm::inverse(rotations[part.Joint]);
				part.OffsetPosition = inverseJoint * (shape.Position - positions[part.Joint]);
				part.OffsetRotation = glm::normalize(inverseJoint * shape.Rotation);
			}

			for (size_t p = 0; p < runtime.Parts.size(); ++p)
			{
				RagdollRuntime::Part& part = runtime.Parts[p];
				float inertia = 0.0f;
				for (size_t q = p; q < runtime.Parts.size(); ++q)
				{
					int32_t up = static_cast<int32_t>(q);
					while (up > static_cast<int32_t>(p))
						up = runtime.Parts[up].Parent;
					if (up != static_cast<int32_t>(p))
						continue;

					const float length = lengths[runtime.Parts[q].Joint];
					const glm::vec3 lever = shapes[q].Position - positions[part.Joint];
					inertia += runtime.Parts[q].Mass * (glm::dot(lever, lever) + length * length / 12.0f);
				}
				part.Inertia = inertia;
				const float length = lengths[part.Joint];
				const glm::vec3 lever = shapes[p].Position - positions[part.Joint];
				part.BodyInertia = part.Mass * (glm::dot(lever, lever) + length * length / 12.0f);

				if (part.Parent < 0)
					continue;

				const RagdollRuntime::Part& parent = runtime.Parts[part.Parent];
				const Shape& shapeA = shapes[part.Parent];
				const Shape& shapeB = shapes[p];
				const glm::quat inverseA = glm::inverse(shapeA.Rotation);
				const glm::quat inverseB = glm::inverse(shapeB.Rotation);
				const glm::vec3 anchor = positions[part.Joint];
				const glm::vec3 directionA = shapeA.Rotation * glm::vec3(0.0f, 1.0f, 0.0f);
				const glm::vec3 directionB = shapeB.Rotation * glm::vec3(0.0f, 1.0f, 0.0f);
				const RagdollJointRecipe* entry = entries[p];
				JointClass type = Classify(names[part.Joint]);
				if (entry && entry->Type == RagdollJointType::Hinge)
					type = JointClass::Hinge;
				else if (entry && entry->Type == RagdollJointType::SwingTwist && type == JointClass::Hinge)
					type = JointClass::Other;
				part.Hinge = type == JointClass::Hinge;

				ConstraintParams3D params(type == JointClass::Hinge ? ConstraintType3D::Hinge : ConstraintType3D::SwingTwist, parent.Body, part.Body);
				params.AnchorA = inverseA * (anchor - shapeA.Position);
				params.AnchorB = inverseB * (anchor - shapeB.Position);
				params.UserData = static_cast<std::uint32_t>(handle);
				if (type == JointClass::Hinge)
				{
					// Bent, the hinge turns the way it already bends: a positive angle bends it further.
					// Straight, the joint's own X axis is the best guess at the bend axis.
					const glm::vec3 bendAxis = glm::cross(directionA, directionB);
					const float bend = std::atan2(glm::length(bendAxis), glm::dot(directionA, directionB));
					glm::vec3 axis;
					const bool limited = entry && entry->HasHingeLimits;
					const float lowest = limited ? entry->HingeMin : -k_HingeOverextension;
					const float highest = limited ? entry->HingeMax : k_HingeMaxBend;
					if (bend > glm::radians(10.0f))
					{
						// The limits are from straight; the build pose sits inside them whatever they say.
						axis = glm::normalize(bendAxis);
						params.MinLimit = std::clamp(lowest - bend, -glm::pi<float>(), 0.0f);
						params.MaxLimit = std::clamp(highest - bend, 0.0f, glm::pi<float>());
					}
					else
					{
						// Straight, nothing says which way it bends: a recipe's positive side turns about this
						// axis, and without one it gets the full bend both ways.
						axis = Perpendicular(rotations[part.Joint] * glm::vec3(1.0f, 0.0f, 0.0f), directionB, rotations[part.Joint] * glm::vec3(0.0f, 0.0f, 1.0f));
						params.MinLimit = limited ? std::clamp(lowest, -glm::pi<float>(), 0.0f) : -k_HingeMaxBend;
						params.MaxLimit = limited ? std::clamp(highest, 0.0f, glm::pi<float>()) : k_HingeMaxBend;
					}
					const glm::vec3 normal = Perpendicular(directionB, axis, directionA);
					params.AxisA = inverseA * axis;
					params.AxisB = inverseB * axis;
					params.NormalAxisA = inverseA * normal;
					params.NormalAxisB = inverseB * normal;
				}
				else
				{
					SwingTwistLimits limits = LimitsOf(type);
					float twistMin = -limits.Twist;
					float twistMax = limits.Twist;
					if (entry && entry->HasSwing)
					{
						limits.SwingY = entry->SwingY;
						limits.SwingZ = entry->SwingZ;
					}
					if (entry && entry->HasTwist)
					{
						twistMin = entry->TwistMin;
						twistMax = entry->TwistMax;
					}
					const glm::vec3 normal = Perpendicular(rotations[part.Joint] * glm::vec3(1.0f, 0.0f, 0.0f), directionB, rotations[part.Joint] * glm::vec3(0.0f, 0.0f, 1.0f));
					params.AxisA = inverseA * directionB;
					params.AxisB = inverseB * directionB;
					params.NormalAxisA = inverseA * normal;
					params.NormalAxisB = inverseB * normal;
					params.SwingLimitY = limits.SwingY;
					params.SwingLimitZ = limits.SwingZ;
					params.TwistMin = twistMin;
					params.TwistMax = twistMax;
				}

				part.Constraint = physics.CreateConstraint(params);
				if (part.Constraint == k_InvalidConstraint3D)
				{
					fail("a joint could not be created");
					return;
				}
			}
		}

		void RagdollSystem::Drive(entt::registry& registry, Physics3D& physics, entt::entity handle, float deltaTime)
		{
			const RagdollComponent* settings = registry.try_get<RagdollComponent>(handle);
			const Skeleton* skeleton = SkeletonOf(registry, handle);
			if (!settings || !skeleton)
				return;

			RagdollRuntime& runtime = registry.get<RagdollRuntime>(handle);
			const bool driven = Driven(*settings);
			runtime.DrivenFor = driven ? runtime.DrivenFor + deltaTime : 0.0f;
			glm::mat4 entityWorld;
			if (driven && !AnimatedJointWorlds(registry, handle, *skeleton, entityWorld))
				return;

			Transform3DComponent* transform = registry.try_get<Transform3DComponent>(handle);
			float groundY = 0.0f;
			if (driven && runtime.Carried)
			{
				runtime.Carried = false;
				runtime.Seating = transform && runtime.HasGroundHeight && GroundUnderRoot(physics, runtime, groundY);
				if (runtime.Seating)
					runtime.SeatHeight = groundY + runtime.GroundHeight;
			}
			if (driven && runtime.Seating && transform)
			{
				glm::vec3 seated(entityWorld[3]);
				const float step = k_SeatSpeed * deltaTime;
				const float remaining = runtime.SeatHeight - seated.y;
				if (std::abs(remaining) <= step)
				{
					seated.y = runtime.SeatHeight;
					runtime.Seating = false;
				}
				else
					seated.y += remaining > 0.0f ? step : -step;
				HierarchySystem::SetWorldPosition(registry, handle, *transform, seated);
				if (!AnimatedJointWorlds(registry, handle, *skeleton, entityWorld))
					return;
			}
			if (!driven)
				runtime.Seating = false;

			const float strength = (std::isfinite(settings->Strength) ? settings->Strength : 0.0f) * (runtime.DrivenFor < k_SoftSeconds ? k_SoftStrength : 1.0f);
			const float frequency = (std::min)(settings->DriveFrequency, k_MaxDriveFrequency);
			const float omega = glm::two_pi<float>() * frequency;
			const float maxBodyOmega = deltaTime > 0.0f ? glm::two_pi<float>() * k_MaxBodyFrequencyPerStep / deltaTime : omega;
			for (RagdollRuntime::Part& part : runtime.Parts)
			{
				if (part.Parent < 0)
					continue;

				MotorParams3D motor;
				MotorMode3D mode = MotorMode3D::Off;
				if (driven)
				{
					const float stiffness = part.Inertia * omega * omega * strength;
					motor.Stiffness = (std::min)(stiffness, part.BodyInertia * maxBodyOmega * maxBodyOmega);
					motor.Damping = 2.0f * (std::sqrt)(motor.Stiffness * part.Inertia);
					motor.MaxTorque = 2.0f * stiffness;
					mode = MotorMode3D::Position;
				}
				part.MaxTorque = motor.MaxTorque;

				if (part.Hinge)
					physics.SetMotor(part.Constraint, MotorAxis3D::Hinge, mode, motor);
				else
				{
					physics.SetMotor(part.Constraint, MotorAxis3D::Swing, mode, motor);
					physics.SetMotor(part.Constraint, MotorAxis3D::Twist, mode, motor);
				}

				if (!driven)
					continue;

				const RagdollRuntime::Part& parent = runtime.Parts[part.Parent];
				glm::vec3 position;
				glm::quat jointA, jointB;
				RigidFrame(m_JointWorlds[parent.Joint], position, jointA);
				RigidFrame(m_JointWorlds[part.Joint], position, jointB);
				const glm::quat bodyA = jointA * parent.OffsetRotation;
				const glm::quat bodyB = jointB * part.OffsetRotation;
				physics.SetMotorTargetRotation(part.Constraint, glm::normalize(glm::inverse(bodyA) * bodyB));
			}

			if (!driven)
			{
				runtime.HasTarget = false;
				return;
			}

			const RagdollRuntime::Part& root = runtime.Parts.front();
			glm::vec3 jointPosition;
			glm::quat jointRotation;
			RigidFrame(m_JointWorlds[root.Joint], jointPosition, jointRotation);
			const glm::vec3 targetPosition = jointPosition + jointRotation * root.OffsetPosition;
			const glm::quat targetRotation = glm::normalize(jointRotation * root.OffsetRotation);

			const glm::vec3 position = physics.GetPosition(root.Body);
			glm::quat error = targetRotation * glm::inverse(physics.GetRotation(root.Body));
			if (error.w < 0.0f)
				error = -error;
			const float angle = 2.0f * std::acos(std::clamp(error.w, -1.0f, 1.0f));
			const glm::vec3 axis = glm::vec3(error.x, error.y, error.z);
			const float axisLength = glm::length(axis);
			const float gain = (std::min)(omega, 1.0f / deltaTime);
			glm::vec3 angular = axisLength > 1e-6f ? axis / axisLength * (angle * gain) : glm::vec3(0.0f);
			glm::vec3 linear = (targetPosition - position) * gain;

			// The target's own velocity, so a moving animation is matched rather than trailed by v/omega.
			// A jump further than k_MaxTargetStep or k_MaxTargetTurn is a teleport and is closed by the spring alone.
			const glm::vec3 targetStep = targetPosition - runtime.TargetPosition;
			if (runtime.HasTarget && deltaTime > 0.0f && glm::dot(targetStep, targetStep) < k_MaxTargetStep * k_MaxTargetStep)
			{
				glm::quat turn = targetRotation * glm::inverse(runtime.TargetRotation);
				if (turn.w < 0.0f)
					turn = -turn;
				const glm::vec3 turnAxis(turn.x, turn.y, turn.z);
				const float turnLength = glm::length(turnAxis);
				const float turnAngle = 2.0f * std::acos(std::clamp(turn.w, -1.0f, 1.0f));
				linear += targetStep / deltaTime;
				if (turnLength > 1e-6f && turnAngle < k_MaxTargetTurn)
					angular += turnAxis / turnLength * (turnAngle / deltaTime);
			}
			runtime.TargetPosition = targetPosition;
			runtime.TargetRotation = targetRotation;
			runtime.HasTarget = true;
			if (!Finite(linear) || !Finite(angular))
				return;

			// The whole ragdoll takes the root's correction as one rigid motion: on the root alone the
			// solve hands it to the heavier chain below, and the body swings on a pinned hip. Its
			// current motion is the mass-weighted mean, since the root's own velocity carries the
			// reaction of a swinging limb and correcting by it feeds the swing.
			float mass = 0.0f;
			glm::vec3 centre(0.0f), momentum(0.0f), spin(0.0f);
			for (const RagdollRuntime::Part& part : runtime.Parts)
			{
				mass += part.Mass;
				centre += part.Mass * physics.GetPosition(part.Body);
				momentum += part.Mass * physics.GetLinearVelocity(part.Body);
				spin += part.Mass * physics.GetAngularVelocity(part.Body);
			}
			if (!(mass > 0.0f))
				return;
			const glm::vec3 meanAngular = spin / mass;
			const glm::vec3 meanLinear = momentum / mass - glm::cross(meanAngular, centre / mass - position);

			const float follow = std::clamp(strength, 0.0f, 1.0f);
			const glm::vec3 deltaLinear = (linear - meanLinear) * follow;
			const glm::vec3 deltaAngular = (angular - meanAngular) * follow;
			for (const RagdollRuntime::Part& part : runtime.Parts)
			{
				const glm::vec3 offset = physics.GetPosition(part.Body) - position;
				physics.SetLinearVelocity(part.Body, physics.GetLinearVelocity(part.Body) + deltaLinear + glm::cross(deltaAngular, offset));
				physics.SetAngularVelocity(part.Body, physics.GetAngularVelocity(part.Body) + deltaAngular);
			}
		}

		void RagdollSystem::AfterStep(entt::registry& registry, Physics3D& physics)
		{
			m_Scratch.clear();
			for (entt::entity handle : registry.view<RagdollRuntime>())
				m_Scratch.push_back(handle);

			for (entt::entity handle : m_Scratch)
			{
				RagdollRuntime& runtime = registry.get<RagdollRuntime>(handle);
				const RagdollComponent* settings = registry.try_get<RagdollComponent>(handle);
				const Skeleton* skeleton = SkeletonOf(registry, handle);
				Transform3DComponent* transform = registry.try_get<Transform3DComponent>(handle);
				if (settings && skeleton && transform && skeleton->GetId() == runtime.SkeletonId)
				{
					const float blend = std::isfinite(settings->Blend) ? std::clamp(settings->Blend, 0.0f, 1.0f) : 0.0f;
					const bool wasFollowing = runtime.FollowsRoot;
					runtime.FollowsRoot = blend > 0.0f && !Driven(*settings)
						&& !registry.any_of<RigidBody3DComponent, CharacterController3DComponent>(handle);
					glm::mat4 entityWorld;
					if (runtime.FollowsRoot && AnimatedJointWorlds(registry, handle, *skeleton, entityWorld))
					{
						if (!wasFollowing)
						{
							float groundY = 0.0f;
							runtime.HasGroundHeight = GroundUnderRoot(physics, runtime, groundY);
							runtime.GroundHeight = runtime.HasGroundHeight ? entityWorld[3].y - groundY : 0.0f;
						}
						runtime.Carried = true;
						const RagdollRuntime::Part& root = runtime.Parts.front();
						const glm::quat rotation = glm::normalize(physics.GetRotation(root.Body) * glm::inverse(root.OffsetRotation));
						const glm::vec3 simulated = physics.GetPosition(root.Body) - rotation * root.OffsetPosition;
						const glm::vec3 shift = simulated - glm::vec3(m_JointWorlds[root.Joint][3]);
						if (Finite(shift) && glm::dot(shift, shift) > 0.0f)
							HierarchySystem::SetWorldPosition(registry, handle, *transform, glm::vec3(entityWorld[3]) + shift);
					}
				}

				for (RagdollRuntime::Part& part : runtime.Parts)
				{
					part.PreviousPosition = part.Position;
					part.PreviousRotation = part.Rotation;
					part.Position = physics.GetPosition(part.Body);
					part.Rotation = physics.GetRotation(part.Body);
					if (!runtime.Recorded)
					{
						part.PreviousPosition = part.Position;
						part.PreviousRotation = part.Rotation;
					}
				}
				runtime.Recorded = true;
			}
		}

		void RagdollSystem::Pose(entt::registry& registry, Physics3D& physics, float alpha)
		{
			m_Scratch.clear();
			for (entt::entity handle : registry.view<RagdollRuntime>())
				m_Scratch.push_back(handle);

			for (entt::entity handle : m_Scratch)
			{
				RagdollRuntime& runtime = registry.get<RagdollRuntime>(handle);
				const RagdollComponent* settings = registry.try_get<RagdollComponent>(handle);
				const Skeleton* skeleton = SkeletonOf(registry, handle);
				if (!settings || !skeleton || skeleton->GetId() != runtime.SkeletonId)
					continue;

				const float blend = std::isfinite(settings->Blend) ? std::clamp(settings->Blend, 0.0f, 1.0f) : 0.0f;
				if (blend <= 0.0f)
					continue;

				Animator* animator = PosingAnimator(registry, handle, *skeleton);
				glm::mat4 entityWorld;
				if (!animator || !AnimatedJointWorlds(registry, handle, *skeleton, entityWorld))
					continue;

				const bool between = alpha >= 0.0f && runtime.Recorded;
				const float t = (std::min)(alpha, 1.0f);
				const size_t count = skeleton->GetJointCount();
				m_Weights.assign(count, 0.0f);
				const glm::mat4 toModel = glm::inverse(entityWorld * skeleton->GetRootTransform());
				for (const RagdollRuntime::Part& part : runtime.Parts)
				{
					const glm::quat bodyRotation = between ? glm::slerp(part.PreviousRotation, part.Rotation, t) : physics.GetRotation(part.Body);
					const glm::vec3 bodyPosition = between ? glm::mix(part.PreviousPosition, part.Position, t) : physics.GetPosition(part.Body);
					const glm::quat rotation = glm::normalize(bodyRotation * glm::inverse(part.OffsetRotation));
					const glm::vec3 position = bodyPosition - rotation * part.OffsetPosition;
					if (!Finite(position) || !std::isfinite(rotation.w) || !Finite(glm::vec3(rotation.x, rotation.y, rotation.z)))
						continue;

					const glm::mat4& animated = m_JointWorlds[part.Joint];
					const glm::vec3 scale(glm::length(glm::vec3(animated[0])), glm::length(glm::vec3(animated[1])), glm::length(glm::vec3(animated[2])));
					const glm::mat4 world = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
					m_Globals[part.Joint] = toModel * world;
					m_Weights[part.Joint] = blend;
				}

				animator->BlendGlobalTransforms(m_Globals, m_Weights);
			}
		}

		void RagdollSystem::Drop(entt::registry& registry, Physics3D* physics, entt::entity handle)
		{
			m_Rejected.erase(handle);
			RagdollRuntime* runtime = registry.try_get<RagdollRuntime>(handle);
			if (!runtime)
				return;

			if (physics)
			{
				for (auto part = runtime->Parts.rbegin(); part != runtime->Parts.rend(); ++part)
				{
					physics->DestroyConstraint(part->Constraint);
					physics->DestroyBody(part->Body);
				}
			}
			registry.remove<RagdollRuntime>(handle);
		}

		void RagdollSystem::Clear(entt::registry& registry)
		{
			m_Rejected.clear();
			registry.clear<RagdollRuntime>();
		}

		bool RagdollSystem::OwnsBody(const entt::registry& registry, entt::entity handle, PhysicsBodyId3D body) const
		{
			const RagdollRuntime* runtime = registry.valid(handle) ? registry.try_get<RagdollRuntime>(handle) : nullptr;
			return runtime && std::any_of(runtime->Parts.begin(), runtime->Parts.end(), [body](const RagdollRuntime::Part& part) { return part.Body == body; });
		}

		PhysicsBodyId3D RagdollSystem::Body(const entt::registry& registry, entt::entity handle, std::string_view joint) const
		{
			const RagdollRuntime* runtime = registry.valid(handle) ? registry.try_get<RagdollRuntime>(handle) : nullptr;
			const Skeleton* skeleton = runtime ? SkeletonOf(registry, handle) : nullptr;
			if (!skeleton || skeleton->GetId() != runtime->SkeletonId)
				return k_InvalidBody3D;

			const int32_t index = skeleton->FindJoint(joint);
			if (index == Skeleton::k_InvalidJoint || static_cast<size_t>(index) >= runtime->PartOfJoint.size())
				return k_InvalidBody3D;

			const int32_t part = runtime->PartOfJoint[index];
			return part >= 0 ? runtime->Parts[part].Body : k_InvalidBody3D;
		}

	}

}
