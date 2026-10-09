#include "ThroughputTest.h"

#include <imgui.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <format>
#include <limits>

namespace
{
	using namespace Dingo;

	using Clock = std::chrono::steady_clock;
	using Mode = ThroughputTest::Mode;

	constexpr uint32_t k_DefaultCount = 10000;
	constexpr uint32_t k_DefaultSkinnedCount = 64;
	constexpr uint32_t k_WarmupFrames = 120;
	constexpr uint32_t k_MeasuredFrames = 600;
	constexpr uint32_t k_VisibilitySamples = 30;
	constexpr float k_Step = 1.0f / 60.0f;

	constexpr float k_CellSize = 2.0f;
	constexpr uint32_t k_RoomCells = 10;
	constexpr float k_FloorChance = 0.45f;
	constexpr float k_UniqueChance = 0.55f;

	// The lap of mixed and shadow, relative to the layout's side: a circle at 0.3 of it, seeing
	// 0.5 of it ahead, which keeps about a tenth of the entities in view.
	constexpr uint32_t k_LapFrames = 1200;
	constexpr float k_LapRadius = 0.3f;
	constexpr float k_ViewDistance = 0.5f;
	constexpr float k_FlyHeight = 3.5f;

	constexpr const char* k_FoxPath = "assets/models/Fox/Fox.gltf";
	constexpr const char* k_FoxClip = "Walk";
	constexpr float k_FoxLength = 1.6f;
	constexpr float k_FoxSpacing = 2.2f;

	constexpr uint32_t k_ShadowLights = 8;

	enum Prop : int32_t { Crate, Pillar, Torch, Rock, Barrel, PropCount };
	constexpr float k_CrateSize = 0.9f;
	constexpr float k_RockHeight = 0.7f;

	const char* ModeName(Mode mode)
	{
		switch (mode)
		{
			case Mode::Static:  return "static";
			case Mode::Repeat:  return "repeat";
			case Mode::Mixed:   return "mixed";
			case Mode::Skinned: return "skinned";
			case Mode::Shadow:  return "shadow";
		}
		return "?";
	}

	bool IsLap(Mode mode) { return mode == Mode::Mixed || mode == Mode::Shadow; }

	double Milliseconds(Clock::time_point from, Clock::time_point to)
	{
		return std::chrono::duration<double, std::milli>(to - from).count();
	}

	// SplitMix64, so every standard library builds the same layout from a seed (<random>'s
	// distributions are implementation-defined).
	class Random
	{
	public:
		explicit Random(uint64_t seed) : m_State(seed) {}

		uint64_t Next()
		{
			uint64_t z = (m_State += 0x9E3779B97F4A7C15ull);
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
			return z ^ (z >> 31);
		}

		float Float() { return static_cast<float>(Next() >> 40) * (1.0f / 16777216.0f); }
		float Range(float low, float high) { return low + (high - low) * Float(); }
		uint32_t Below(uint32_t count) { return static_cast<uint32_t>(Next() % count); }

	private:
		uint64_t m_State;
	};

	class Fnv1a
	{
	public:
		template<typename T>
		void Add(const T& value)
		{
			const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&value);
			for (size_t i = 0; i < sizeof(T); ++i)
				m_Hash = (m_Hash ^ bytes[i]) * 1099511628211ull;
		}

		uint64_t Get() const { return m_Hash; }

	private:
		uint64_t m_Hash = 1469598103934665603ull;
	};

	uint32_t GridColumns(uint32_t count)
	{
		return (std::max)(1u, static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(count)))));
	}

	glm::vec4 RandomColor(Random& random)
	{
		const float r = random.Range(0.35f, 0.95f);
		const float g = random.Range(0.35f, 0.95f);
		const float b = random.Range(0.35f, 0.95f);
		return { r, g, b, 1.0f };
	}

	ThroughputTest::Prism RandomPrism(Random& random)
	{
		ThroughputTest::Prism prism;
		prism.Sides = 3 + random.Below(7);
		prism.BottomRadius = random.Range(0.3f, 0.85f);
		prism.TopRadius = random.Range(0.1f, 0.85f);
		prism.Height = random.Range(0.4f, 2.2f);
		prism.Twist = random.Range(0.0f, glm::pi<float>() / static_cast<float>(prism.Sides));
		return prism;
	}

	void SetProp(ThroughputTest::Placement& placement, int32_t prop)
	{
		placement.Prop = prop;
		if (prop == Crate)
			placement.Position.y = 0.5f * k_CrateSize;
		else if (prop == Rock)
		{
			placement.Scale = { 1.0f, k_RockHeight, 1.0f };
			placement.Position.y = 0.5f * k_RockHeight;
		}
	}

	ThroughputTest::Layout GenerateLayout(Mode mode, uint32_t count, uint64_t seed)
	{
		ThroughputTest::Layout layout;
		layout.Placements.reserve(count);
		Random random(seed);

		const uint32_t columns = GridColumns(count);
		const uint32_t rows = (count + columns - 1) / columns;
		const float spacing = mode == Mode::Skinned ? k_FoxSpacing : k_CellSize;
		for (uint32_t i = 0; i < count; ++i)
		{
			const uint32_t column = i % columns;
			const uint32_t row = i / columns;

			ThroughputTest::Placement placement;
			placement.Position = { (static_cast<float>(column) - 0.5f * static_cast<float>(columns - 1)) * spacing, 0.0f,
				(static_cast<float>(row) - 0.5f * static_cast<float>(rows - 1)) * spacing };
			placement.Yaw = random.Range(0.0f, glm::two_pi<float>());
			placement.Color = RandomColor(random);

			switch (mode)
			{
				case Mode::Static:
					placement.Unique = static_cast<uint32_t>(layout.Prisms.size());
					layout.Prisms.push_back(RandomPrism(random));
					break;

				case Mode::Repeat:
					SetProp(placement, static_cast<int32_t>(random.Below(PropCount)));
					break;

				case Mode::Mixed:
				case Mode::Shadow:
				{
					const bool wallColumn = column % k_RoomCells == 0;
					const bool wallRow = row % k_RoomCells == 0;
					const bool door = (wallColumn && row % k_RoomCells == k_RoomCells / 2) || (wallRow && column % k_RoomCells == k_RoomCells / 2);
					const float roll = random.Float();
					if ((wallColumn || wallRow) && !door)
						SetProp(placement, Pillar);
					else if (roll < k_FloorChance)
					{
						placement.Prop = Crate;
						placement.Yaw = 0.0f;
						placement.Scale = { k_CellSize / k_CrateSize, 0.1f / k_CrateSize, k_CellSize / k_CrateSize };
						placement.Position.y = -0.05f;
					}
					else if (random.Float() < k_UniqueChance)
					{
						placement.Unique = static_cast<uint32_t>(layout.Prisms.size());
						layout.Prisms.push_back(RandomPrism(random));
					}
					else
					{
						constexpr int32_t clutter[] = { Crate, Torch, Rock, Barrel };
						SetProp(placement, clutter[random.Below(4)]);
					}
					break;
				}

				case Mode::Skinned:
					break;
			}
			layout.Placements.push_back(placement);
		}
		return layout;
	}

	uint64_t HashLayout(const ThroughputTest::Layout& layout)
	{
		Fnv1a hash;
		for (const ThroughputTest::Placement& placement : layout.Placements)
		{
			hash.Add(placement.Position);
			hash.Add(placement.Yaw);
			hash.Add(placement.Scale);
			hash.Add(placement.Color);
			hash.Add(placement.Prop);
			hash.Add(placement.Unique);
		}
		for (const ThroughputTest::Prism& prism : layout.Prisms)
		{
			hash.Add(prism.Sides);
			hash.Add(prism.BottomRadius);
			hash.Add(prism.TopRadius);
			hash.Add(prism.Height);
			hash.Add(prism.Twist);
		}
		return hash.Get();
	}

	void HashTransform(Fnv1a& hash, const Transform3DComponent& transform)
	{
		hash.Add(transform.Position);
		hash.Add(transform.Rotation);
		hash.Add(transform.Scale);
	}

	// A convex face, flat shaded and counter-clockwise seen from outside.
	void AddFace(std::vector<MeshVertex>& vertices, std::vector<uint32_t>& indices, std::vector<glm::vec3> corners, const glm::vec3& outward)
	{
		glm::vec3 normal = glm::cross(corners[1] - corners[0], corners[2] - corners[0]);
		if (glm::dot(normal, outward) < 0.0f)
		{
			std::reverse(corners.begin(), corners.end());
			normal = -normal;
		}
		normal = glm::normalize(normal);

		const uint32_t base = static_cast<uint32_t>(vertices.size());
		for (const glm::vec3& corner : corners)
			vertices.push_back({ corner, normal, { corner.x + corner.z, corner.y } });
		for (uint32_t i = 1; i + 1 < corners.size(); ++i)
			indices.insert(indices.end(), { base, base + i, base + i + 1 });
	}

	Mesh* BuildPrism(const ThroughputTest::Prism& prism)
	{
		std::vector<MeshVertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<glm::vec3> bottom(prism.Sides);
		std::vector<glm::vec3> top(prism.Sides);
		for (uint32_t i = 0; i < prism.Sides; ++i)
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(prism.Sides);
			bottom[i] = { prism.BottomRadius * std::cos(angle), 0.0f, prism.BottomRadius * std::sin(angle) };
			top[i] = { prism.TopRadius * std::cos(angle + prism.Twist), prism.Height, prism.TopRadius * std::sin(angle + prism.Twist) };
		}

		for (uint32_t i = 0; i < prism.Sides; ++i)
		{
			const uint32_t next = (i + 1) % prism.Sides;
			const glm::vec3 middle = 0.25f * (bottom[i] + bottom[next] + top[i] + top[next]);
			AddFace(vertices, indices, { bottom[i], bottom[next], top[next], top[i] }, glm::vec3(middle.x, 0.0f, middle.z));
		}
		AddFace(vertices, indices, bottom, { 0.0f, -1.0f, 0.0f });
		AddFace(vertices, indices, top, { 0.0f, 1.0f, 0.0f });
		return Mesh::Create(vertices, indices);
	}

	std::string TimerKey(const char* name)
	{
		std::string key;
		for (const char* c = name; c && *c; ++c)
			key += std::isalnum(static_cast<unsigned char>(*c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(*c))) : '_';
		return key;
	}
}

namespace Dingo
{

	void ThroughputTest::ReadArgs()
	{
		m_ArgsRead = true;
		const ApplicationCommandLineArgs& args = Application::Get().GetCommandLineArgs();
		if (auto mode = args.Get("throughput"))
		{
			m_Mode = *mode == "repeat" ? Mode::Repeat : *mode == "mixed" ? Mode::Mixed
				: *mode == "skinned" ? Mode::Skinned : *mode == "shadow" ? Mode::Shadow : Mode::Static;
		}
		if (auto count = args.Get("count"))
			m_Count = static_cast<uint32_t>((std::max)(1l, std::strtol(std::string(*count).c_str(), nullptr, 10)));
		if (auto seed = args.Get("seed"))
			m_Seed = std::strtoull(std::string(*seed).c_str(), nullptr, 10);
		m_Perf = args.Get("perf").has_value();
		if (auto frame = args.Get("frame"))
			m_HeldFrame = static_cast<int64_t>(std::strtoll(std::string(*frame).c_str(), nullptr, 10));
	}

	void ThroughputTest::Initialize()
	{
		if (!m_ArgsRead)
			ReadArgs();
		if (m_Count == 0)
			m_Count = m_Mode == Mode::Skinned ? k_DefaultSkinnedCount : k_DefaultCount;

		m_Checks.clear();
		m_ChecksDone = false;
		m_Frame = 0;
		m_PerfFrames = 0;
		m_PerfDone = false;
		m_PerfFrameMs = m_PerfUpdateMs = m_PerfRenderEntitiesMs = m_PerfEndSceneMs = m_PerfDraws = m_PerfVertices = 0.0;
		m_PerfDropped = 0;
		m_PerfGpuMs.clear();
		m_PerfResult.clear();

		if (m_Mode == Mode::Skinned)
		{
			m_Fox = Model::LoadFromFile(k_FoxPath);
			Check(m_Fox && m_Fox->IsSkinned() && m_Fox->FindAnimation(k_FoxClip), "Fox.gltf loads with a skeleton and its Walk clip");
			if (!m_Fox || !m_Fox->IsSkinned())
				return;

			glm::vec3 minBounds((std::numeric_limits<float>::max)());
			glm::vec3 maxBounds((std::numeric_limits<float>::lowest)());
			for (const SubMesh& submesh : m_Fox->GetSubMeshes())
			{
				minBounds = (glm::min)(minBounds, submesh.MeshData->GetBoundsMin());
				maxBounds = (glm::max)(maxBounds, submesh.MeshData->GetBoundsMax());
			}
			const glm::vec3 extent = maxBounds - minBounds;
			const glm::vec3 center = 0.5f * (minBounds + maxBounds);
			m_FoxScale = k_FoxLength / (std::max)({ extent.x, extent.y, extent.z });
			m_FoxOffset = -glm::vec3(center.x, minBounds.y, center.z) * m_FoxScale;
			m_FoxSubMeshes = static_cast<uint32_t>(m_Fox->GetSubMeshes().size());

			m_FoxMaterial = Application::Get().GetRenderer3D().CreateLitMaterial(MaterialParams().SetDebugName("ThroughputFox"));
			for (const SubMesh& submesh : m_Fox->GetSubMeshes())
			{
				if (submesh.MeshData->HasSkin() && submesh.DiffuseTexture)
				{
					m_FoxMaterial->SetTexture(0, submesh.DiffuseTexture);
					break;
				}
			}
		}

		BuildScene();
	}

	void ThroughputTest::BuildScene()
	{
		const Clock::time_point start = Clock::now();
		m_Layout = GenerateLayout(m_Mode, m_Count, m_Seed);
		m_LayoutHash = HashLayout(m_Layout);
		m_Extent = static_cast<float>(GridColumns(m_Count)) * (m_Mode == Mode::Skinned ? k_FoxSpacing : k_CellSize);

		if (m_Mode != Mode::Static && m_Mode != Mode::Skinned)
		{
			m_Props.resize(PropCount);
			m_Props[Crate] = Mesh::CreateBox(k_CrateSize, k_CrateSize, k_CrateSize);
			m_Props[Pillar] = BuildPrism({ 8, 0.35f, 0.3f, 3.0f, 0.0f });
			m_Props[Torch] = BuildPrism({ 6, 0.08f, 0.14f, 1.6f, 0.0f });
			m_Props[Rock] = Mesh::CreateSphere(0.5f, 6, 8);
			m_Props[Barrel] = BuildPrism({ 12, 0.4f, 0.4f, 1.0f, 0.0f });
		}
		m_UniqueMeshes.reserve(m_Layout.Prisms.size());
		for (const Prism& prism : m_Layout.Prisms)
			m_UniqueMeshes.push_back(BuildPrism(prism));

		m_Scene = new Scene("Throughput Test");
		m_Renderables.reserve(m_Layout.Placements.size());
		for (const Placement& placement : m_Layout.Placements)
		{
			Entity entity = m_Scene->CreateEntity();
			entity.AddComponent<Transform3DComponent>(MakeTransform(placement));
			if (m_Mode == Mode::Skinned)
			{
				if (m_Fox)
				{
					entity.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(m_Fox, placement.Color)).Material = m_FoxMaterial;
					entity.AddComponent<AnimatorComponent>(AnimatorComponent(k_FoxClip));
				}
			}
			else
			{
				Mesh* mesh = placement.Prop >= 0 ? m_Props[placement.Prop] : m_UniqueMeshes[placement.Unique];
				entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, placement.Color));
			}
			m_Renderables.push_back(entity);
		}

		if (m_Mode == Mode::Shadow)
		{
			m_Scene->CreateEntity("Sun").AddComponent<DirectionalLightComponent>().CastShadows = true;
			const float radius = k_LapRadius * m_Extent;
			for (uint32_t i = 0; i < k_ShadowLights; ++i)
			{
				const float angle = glm::two_pi<float>() * (static_cast<float>(i) + 0.5f) / static_cast<float>(k_ShadowLights);
				Entity light = m_Scene->CreateEntity(std::format("Lamp {}", i));
				light.AddComponent<Transform3DComponent>(Transform3DComponent({ radius * std::cos(angle), 3.0f, radius * std::sin(angle) }));
				PointLightComponent& point = light.AddComponent<PointLightComponent>(PointLightComponent({ 1.0f, 0.75f, 0.45f }, 2.0f, 12.0f));
				point.CastShadows = true;
			}
		}

		m_Scene->OnStart();

		if (m_Mode == Mode::Skinned)
		{
			// Out of step, so the crowd doesn't march in unison.
			for (size_t i = 0; i < m_Renderables.size(); ++i)
			{
				if (Animator* animator = m_Scene->GetAnimator(m_Renderables[i]))
				{
					animator->SetTime(0.137f * static_cast<float>(i));
					animator->Update(0.0f);
				}
			}
		}

		PlaceCamera(m_Camera, 0);
		DE_INFO("[Throughput] mode={} count={} seed={} layout=0x{:016x} unique_meshes={} built in {:.1f} ms", ModeName(m_Mode), m_Count, m_Seed,
			m_LayoutHash, m_UniqueMeshes.size(), Milliseconds(start, Clock::now()));
	}

	Transform3DComponent ThroughputTest::MakeTransform(const Placement& placement) const
	{
		const glm::quat rotation = glm::angleAxis(placement.Yaw, glm::vec3(0.0f, 1.0f, 0.0f));
		Transform3DComponent transform(placement.Position, placement.Scale);
		transform.Rotation = rotation;
		if (m_Mode == Mode::Skinned)
		{
			transform.Position += rotation * m_FoxOffset;
			transform.Scale *= m_FoxScale;
		}
		return transform;
	}

	void ThroughputTest::PlaceCamera(PerspectiveCamera& camera, uint64_t frame) const
	{
		if (IsLap(m_Mode))
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(frame % k_LapFrames) / static_cast<float>(k_LapFrames);
			const float radius = k_LapRadius * m_Extent;
			const glm::vec3 position(radius * std::cos(angle), k_FlyHeight, radius * std::sin(angle));
			const glm::vec3 ahead(-std::sin(angle), 0.0f, std::cos(angle));
			camera.SetClip(0.1f, k_ViewDistance * m_Extent);
			camera.SetPosition(position);
			camera.SetTarget(position + 10.0f * ahead - glm::vec3(0.0f, 2.0f, 0.0f));
			return;
		}

		const float half = 0.5f * m_Extent;
		camera.SetClip(0.1f, 4.0f * m_Extent + 50.0f);
		camera.SetPosition({ -0.9f * half - 2.0f, 1.1f * half + 1.5f, 1.4f * half + 2.5f });
		camera.SetTarget(glm::vec3(0.0f));
	}

	float ThroughputTest::EstimateVisiblePercent() const
	{
		if (m_Layout.Placements.empty())
			return 0.0f;

		PerspectiveCamera camera = m_Camera;
		uint64_t inView = 0;
		for (uint32_t sample = 0; sample < k_VisibilitySamples; ++sample)
		{
			PlaceCamera(camera, k_WarmupFrames + sample * k_MeasuredFrames / k_VisibilitySamples);
			const glm::mat4& viewProjection = camera.GetViewProjectionMatrix();
			for (const Placement& placement : m_Layout.Placements)
			{
				const glm::vec4 clip = viewProjection * glm::vec4(placement.Position, 1.0f);
				inView += std::abs(clip.x) <= clip.w && std::abs(clip.y) <= clip.w && clip.z >= 0.0f && clip.z <= clip.w;
			}
		}
		return 100.0f * static_cast<float>(inView) / static_cast<float>(k_VisibilitySamples * m_Layout.Placements.size());
	}

	void ThroughputTest::Update(float deltaTime)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		PlaceCamera(m_Camera, m_HeldFrame >= 0 ? static_cast<uint64_t>(m_HeldFrame) : m_Frame);

		renderer.BeginScene(m_Camera);
		renderer.Clear(m_ClearColor);
		if (!m_Scene)
		{
			renderer.EndScene();
			return;
		}

		const Clock::time_point updateStart = Clock::now();
		m_Scene->OnUpdate(k_Step);
		const Clock::time_point updateEnd = Clock::now();
		m_Scene->SubmitLights(renderer);
		const Clock::time_point renderStart = Clock::now();
		m_Scene->RenderEntities3D(renderer);
		const Clock::time_point endSceneStart = Clock::now();
		renderer.EndScene();
		const Clock::time_point endSceneEnd = Clock::now();

		const bool skipped = Renderer::IsFrameSkipped();
		if (!skipped)
		{
			m_LastStats = renderer.GetStatistics();
			if (!m_ChecksDone)
				RunChecks(m_LastStats);

			const double renderMs = Milliseconds(renderStart, endSceneStart);
			const double endSceneMs = Milliseconds(endSceneStart, endSceneEnd);
			m_RollingFrameMs += (1000.0 * deltaTime - m_RollingFrameMs) * 0.05;
			m_RollingRenderMs += (renderMs - m_RollingRenderMs) * 0.05;
			m_RollingEndSceneMs += (endSceneMs - m_RollingEndSceneMs) * 0.05;
			if (m_Perf && !m_PerfDone && m_Frame >= k_WarmupFrames)
				RecordPerf(deltaTime, Milliseconds(updateStart, updateEnd), renderMs, endSceneMs, m_LastStats);
		}
		++m_Frame;
	}

	void ThroughputTest::RunChecks(const Renderer3D::Statistics& stats)
	{
		m_ChecksDone = true;

		uint32_t renderables = 0;
		m_Scene->ForEachEntity([&renderables](Entity entity)
		{
			renderables += entity.HasComponent<MeshRendererComponent>() || entity.HasComponent<SkinnedMeshRendererComponent>() ? 1u : 0u;
		});
		Check(renderables == m_Count, std::format("{} {}: the scene holds {} renderable entities", m_Count, ModeName(m_Mode), m_Count));

		Check(stats.DroppedMeshes == 0, std::format("no mesh dropped (DroppedMeshes {})", stats.DroppedMeshes));
		if (m_Mode == Mode::Skinned)
		{
			const uint32_t budget = Application::Get().GetRenderer3D().GetSkinnedInstanceBudget();
			const uint32_t expectedDropped = (m_Count > budget ? m_Count - budget : 0u) * m_FoxSubMeshes;
			Check(stats.DroppedSkinnedDraws == expectedDropped && stats.SkinnedDraws + stats.DroppedSkinnedDraws == m_Count * m_FoxSubMeshes,
				std::format("{} foxes: {} skinned draws, {} dropped past MaxSkinnedInstances ({})", m_Count, stats.SkinnedDraws, stats.DroppedSkinnedDraws, budget));
		}
		else
		{
			Check(stats.SubmittedMeshes == m_Count, std::format("every entity's mesh reaches a batch ({} of {})", stats.SubmittedMeshes, m_Count));
		}

		const uint64_t again = HashLayout(GenerateLayout(m_Mode, m_Count, m_Seed));
		Check(again == m_LayoutHash, std::format("the same seed builds the same layout (0x{:016x})", m_LayoutHash));

		Fnv1a expected;
		Fnv1a built;
		for (size_t i = 0; i < m_Renderables.size(); ++i)
		{
			HashTransform(expected, MakeTransform(m_Layout.Placements[i]));
			HashTransform(built, m_Renderables[i].GetComponent<Transform3DComponent>());
		}
		Check(built.Get() == expected.Get(),
			std::format("the entities' transforms are the layout's (0x{:016x})", built.Get()));
	}

	void ThroughputTest::RecordPerf(float deltaTime, double updateMs, double renderEntitiesMs, double endSceneMs, const Renderer3D::Statistics& stats)
	{
		m_PerfFrameMs += 1000.0 * static_cast<double>(deltaTime);
		m_PerfUpdateMs += updateMs;
		m_PerfRenderEntitiesMs += renderEntitiesMs;
		m_PerfEndSceneMs += endSceneMs;
		m_PerfDraws += static_cast<double>(stats.DrawCalls);
		m_PerfVertices += static_cast<double>(stats.VertexCount);
		m_PerfDropped = (std::max)(m_PerfDropped, stats.DroppedMeshes + stats.DroppedSkinnedDraws);
		for (const GpuTimerStats& timer : Renderer::GetGpuTimers())
			m_PerfGpuMs[TimerKey(timer.Name)] += static_cast<double>(timer.LastMs);
		if (++m_PerfFrames < k_MeasuredFrames)
			return;

		m_PerfDone = true;
		const double frames = static_cast<double>(m_PerfFrames);
		m_PerfResult = std::format("mode={} count={} frame_ms={:.3f} update_ms={:.3f} render_entities_ms={:.3f} end_scene_ms={:.3f} draws={:.0f} vertices={:.0f} dropped={} visible_pct={:.1f}",
			ModeName(m_Mode), m_Count, m_PerfFrameMs / frames, m_PerfUpdateMs / frames, m_PerfRenderEntitiesMs / frames, m_PerfEndSceneMs / frames,
			m_PerfDraws / frames, m_PerfVertices / frames, m_PerfDropped, IsLap(m_Mode) ? EstimateVisiblePercent() : 100.0f);
		for (const auto& [name, total] : m_PerfGpuMs)
			m_PerfResult += std::format(" gpu_{}_ms={:.3f}", name, total / frames);
		DE_INFO("[PERF] throughput {}", m_PerfResult);
	}

	void ThroughputTest::Cleanup()
	{
		m_Renderables.clear();
		delete m_Scene;
		m_Scene = nullptr;

		for (Mesh* mesh : m_UniqueMeshes)
			delete mesh;
		m_UniqueMeshes.clear();
		for (Mesh* mesh : m_Props)
			delete mesh;
		m_Props.clear();
		m_Layout = {};

		DestroyAndDelete(m_FoxMaterial);
		DestroyAndDelete(m_Fox);
	}

	void ThroughputTest::Resize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_Camera.SetAspectRatio(m_AspectRatio);
	}

	void ThroughputTest::ImGuiRender()
	{
		GraphicsTest::ImGuiRender();

		int mode = static_cast<int>(m_Mode);
		ImGui::RadioButton("Static", &mode, static_cast<int>(Mode::Static)); ImGui::SameLine();
		ImGui::RadioButton("Repeat", &mode, static_cast<int>(Mode::Repeat)); ImGui::SameLine();
		ImGui::RadioButton("Mixed", &mode, static_cast<int>(Mode::Mixed));
		ImGui::RadioButton("Skinned", &mode, static_cast<int>(Mode::Skinned)); ImGui::SameLine();
		ImGui::RadioButton("Shadow", &mode, static_cast<int>(Mode::Shadow));
		if (mode != static_cast<int>(m_Mode))
		{
			Application::Get().SubmitPostExecution([this, mode]()
			{
				Cleanup();
				m_Mode = static_cast<Mode>(mode);
				m_Count = 0;
				Initialize();
			});
		}

		ImGui::Text("%u entities, seed %llu, layout 0x%016llx", m_Count, static_cast<unsigned long long>(m_Seed), static_cast<unsigned long long>(m_LayoutHash));
		ImGui::Text("Frame %.2f ms, RenderEntities3D %.3f ms, EndScene %.3f ms", m_RollingFrameMs, m_RollingRenderMs, m_RollingEndSceneMs);
		ImGui::Text("%u draws, %u meshes, %u vertices, %u dropped", m_LastStats.DrawCalls, m_LastStats.SubmittedMeshes, m_LastStats.VertexCount,
			m_LastStats.DroppedMeshes + m_LastStats.DroppedSkinnedDraws);
		if (m_Mode == Mode::Skinned)
			ImGui::Text("%u skinned draws, %u dropped", m_LastStats.SkinnedDraws, m_LastStats.DroppedSkinnedDraws);
		if (m_Mode == Mode::Shadow)
			ImGui::Text("%u shadow views, %u casters, %u shadow draws", m_LastStats.ShadowViews, m_LastStats.ShadowCasters, m_LastStats.ShadowDrawCalls);
		if (m_Perf)
			ImGui::TextWrapped("%s", m_PerfDone ? m_PerfResult.c_str() : std::format("--perf: frame {} of {}", m_Frame, k_WarmupFrames + k_MeasuredFrames).c_str());

		if (!m_Checks.empty() && ImGui::CollapsingHeader("Checks", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (const CheckResult& result : m_Checks)
				ImGui::TextColored(result.Passed ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s %s", result.Passed ? "PASS" : "FAIL", result.Name.c_str());
		}
	}

}
