#include "depch.h"
#include "DingoEngine/Graphics/Renderer3D.h"
#include "DingoEngine/Asset/UnmanagedShaderWatch.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/LightMath.h"

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <array>
#include <cstring>

namespace
{
#include "Renderer3D_Lit.glsl.inl"

	constexpr const char* k_LitShaderName = "Renderer3DMeshShader";
	constexpr const char* k_SkinnedLitShaderName = "Renderer3DSkinnedMeshShader";

	// The source file when this build can see it, so the AssetManager can hot-reload it;
	// otherwise the copy compiled into the library.
	Dingo::Shader* CreateLitShader(const char* name, bool skinned)
	{
		Dingo::ShaderParams params = Dingo::ShaderParams().SetName(name);
		if (skinned)
			params.AddDefine("DE_SKINNED");

#ifdef DE_ENGINE_SHADER_DIR
		const std::filesystem::path sourcePath = std::filesystem::path(u8"" DE_ENGINE_SHADER_DIR) / "Renderer3D_Lit.glsl";
		std::error_code ec;
		if (std::filesystem::exists(sourcePath, ec))
			return Dingo::Shader::Create(params.SetFilePath(sourcePath));
#endif
		const std::string source(reinterpret_cast<const char*>(k_Renderer3D_Lit_glsl), sizeof(k_Renderer3D_Lit_glsl));
		return Dingo::Shader::Create(params.SetSourceCode(source));
	}

	// The camera is the one point a view-projection sends to clip (0, 0, k, 0), so it is the
	// inverse image of that direction. An orthographic camera sits at infinity: w is 0 there,
	// and the result is the direction towards it. A singular matrix gives an arbitrary result.
	glm::vec4 CameraPositionFromViewProjection(const glm::mat4& viewProjection)
	{
		const glm::vec4 eye = glm::inverse(viewProjection) * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
		if (std::abs(eye.w) > 1e-6f)
			return glm::vec4(glm::vec3(eye) / eye.w, 1.0f);

		const float length = glm::length(glm::vec3(eye));
		return length > 0.0f ? glm::vec4(-glm::vec3(eye) / length, 0.0f) : glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
	}

	using FrustumPlanes = std::array<glm::vec4, 6>;

	// Gribb-Hartmann, for clip-space depth in [0, 1] (GLM_FORCE_DEPTH_ZERO_TO_ONE). Each plane's
	// normal points into the frustum.
	FrustumPlanes ExtractFrustumPlanes(const glm::mat4& viewProjection)
	{
		const glm::vec4 x = glm::row(viewProjection, 0);
		const glm::vec4 y = glm::row(viewProjection, 1);
		const glm::vec4 z = glm::row(viewProjection, 2);
		const glm::vec4 w = glm::row(viewProjection, 3);

		FrustumPlanes planes = { w + x, w - x, w + y, w - y, z, w - z };
		for (glm::vec4& plane : planes)
			plane /= glm::length(glm::vec3(plane));
		return planes;
	}

	bool SphereTouchesFrustum(const FrustumPlanes& planes, const glm::vec3& center, float radius)
	{
		for (const glm::vec4& plane : planes)
		{
			if (glm::dot(glm::vec3(plane), center) + plane.w < -radius)
				return false;
		}
		return true;
	}

	// By the strongest channel, not luminance: a pure blue light is as strong as a pure green one
	// of the same intensity, so which of them a budget keeps comes down to distance.
	float Strength(const glm::vec3& color)
	{
		return std::max({ color.r, color.g, color.b });
	}

	using Dingo::Internal::IsFinite;

	float FiniteOr(float value, float fallback)
	{
		return std::isfinite(value) ? value : fallback;
	}

	// A lit material is known by its shader, and the renderer that draws it need not be the one that
	// created it.
	std::vector<const Dingo::Shader*> s_LitShaders;

	bool IsLitShader(const Dingo::Shader* shader)
	{
		return shader && std::find(s_LitShaders.begin(), s_LitShaders.end(), shader) != s_LitShaders.end();
	}

	bool BindsPastSlotZero(const Dingo::Material& material)
	{
		for (uint32_t slot = 1; slot < Dingo::Material::k_MaxTextureSlots; ++slot)
		{
			if (material.GetTexture(slot))
				return true;
		}
		for (uint32_t slot = 1; slot < Dingo::Material::k_MaxSamplerSlots; ++slot)
		{
			if (material.GetSampler(slot))
				return true;
		}
		return false;
	}
}

namespace Dingo
{

	Renderer3D* Renderer3D::Create(const Renderer3DParams& params)
	{
		Renderer3D* renderer = new Renderer3D(params);
		renderer->Initialize();
		return renderer;
	}

	Renderer3D::~Renderer3D()
	{
		std::erase(s_LitShaders, m_Shader);
		Internal::UnwatchUnmanagedShader(m_Shader);
		Internal::UnwatchUnmanagedShader(m_SkinnedShader);
	}

	void Renderer3D::Initialize()
	{
		m_Shader = CreateLitShader(k_LitShaderName, false);
		s_LitShaders.push_back(m_Shader);
		Internal::WatchUnmanagedShader(m_Shader);

		m_SkinnedLayout = VertexLayout()
			.SetStride(sizeof(SkinnedMeshVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(SkinnedMeshVertex, Position))
			.AddAttribute("a_Normal", Format::RGB32_FLOAT, offsetof(SkinnedMeshVertex, Normal))
			.AddAttribute("a_TexCoord", Format::RG32_FLOAT, offsetof(SkinnedMeshVertex, TexCoord))
			.AddAttribute("a_Joints", Format::RGBA16_UINT, offsetof(SkinnedMeshVertex, Joints))
			.AddAttribute("a_Weights", Format::RGBA32_FLOAT, offsetof(SkinnedMeshVertex, Weights));

		m_Layout = VertexLayout()
			.SetStride(sizeof(Vertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(Vertex, Position))
			.AddAttribute("a_Normal", Format::RGB32_FLOAT, offsetof(Vertex, Normal))
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(Vertex, Color))
			.AddAttribute("a_TexCoord", Format::RG32_FLOAT, offsetof(Vertex, TexCoord));

		m_Material = CreateLitMaterial(MaterialParams().SetDebugName("Renderer3D_Material"));

		// Every EndScene writes it, and on Vulkan a volatile buffer only has room for the writes of
		// MaxWritesPerFrame scenes a frame before later ones are dropped.
		m_SceneUniformBuffer = GraphicsBuffer::Create(GraphicsBufferParams()
			.SetDebugName("Renderer3D_SceneUBO")
			.SetByteSize(sizeof(CameraData))
			.SetType(BufferType::UniformBuffer)
			.SetIsVolatile(true)
			.SetDirectUpload(false)
			.SetMaxWritesPerFrame(k_MaxScenesPerFrame));

		// Built-in unit primitives for the DrawBox/DrawSphere conveniences.
		m_BoxMesh = Mesh::CreateBox();
		m_SphereMesh = Mesh::CreateSphere(0.5f, 16, 16);
	}

	void Renderer3D::Shutdown()
	{
		for (GraphicsBuffer*& buffer : m_BatchVertexBuffers)
			DestroyAndDelete(buffer);
		for (GraphicsBuffer*& buffer : m_BatchIndexBuffers)
			DestroyAndDelete(buffer);
		m_BatchVertexBuffers.clear();
		m_BatchIndexBuffers.clear();
		m_Batches.clear();
		m_DrawOrder.clear();

		delete m_BoxMesh;
		delete m_SphereMesh;
		m_BoxMesh = nullptr;
		m_SphereMesh = nullptr;

		for (auto& [id, entry] : m_SkinnedTwins)
			DestroyAndDelete(entry.Twin);
		m_SkinnedTwins.clear();
		m_SkinnedSubmissions.clear();
		m_SkinnedInstances.clear();
		m_SkinnedJoints.clear();
		DestroyAndDelete(m_SkinBuffer);
		Internal::UnwatchUnmanagedShader(m_SkinnedShader);
		DestroyAndDelete(m_SkinnedShader);

		DestroyAndDelete(m_SceneUniformBuffer);
		DestroyAndDelete(m_Material);
		std::erase(s_LitShaders, m_Shader);
		Internal::UnwatchUnmanagedShader(m_Shader);
		DestroyAndDelete(m_Shader);
	}

	void Renderer3D::BeginScene(const PerspectiveCamera& camera)
	{
		BeginSceneInternal(camera.GetViewProjectionMatrix(), glm::vec4(camera.GetPosition(), 1.0f));
	}

	void Renderer3D::BeginScene(const glm::mat4& viewProjection)
	{
		BeginSceneInternal(viewProjection, CameraPositionFromViewProjection(viewProjection));
	}

	void Renderer3D::BeginSceneInternal(const glm::mat4& viewProjection, const glm::vec4& cameraPosition)
	{
		m_CameraData.ViewProjection = viewProjection;
		m_CameraData.CameraPosition = cameraPosition;

		m_Statistics = {};

		// Reset the per-material batches, keeping their storage for reuse. Every chunk, not
		// just last scene's: SubmitMesh takes a spare chunk before growing. Nothing tells the
		// renderer when a material is deleted, so a batch that has sat unused for a while is
		// released instead, or a deleted material's storage would be held forever.
		for (auto it = m_Batches.begin(); it != m_Batches.end();)
		{
			MaterialBatch& matBatch = it->second;
			matBatch.IdleScenes = matBatch.Enqueued ? 0 : matBatch.IdleScenes + 1;
			if (matBatch.IdleScenes > k_MaxIdleBatchScenes)
			{
				it = m_Batches.erase(it);
				continue;
			}

			for (MeshChunk& chunk : matBatch.Chunks)
			{
				chunk.Vertices.clear();
				chunk.Indices.clear();
			}
			matBatch.ChunksInUse = 0;
			matBatch.Enqueued = false;
			++it;
		}
		m_DrawOrder.clear();

		for (auto it = m_SkinnedTwins.begin(); it != m_SkinnedTwins.end();)
		{
			SkinnedTwin& entry = it->second;
			entry.IdleScenes = entry.Used ? 0 : entry.IdleScenes + 1;
			entry.Used = false;
			if (entry.IdleScenes > k_MaxIdleBatchScenes)
			{
				DestroyAndDelete(entry.Twin);
				it = m_SkinnedTwins.erase(it);
				continue;
			}
			++it;
		}
		m_SkinnedSubmissions.clear();
		m_SkinnedInstances.clear();
		m_SkinnedJoints.clear();

		m_SceneActive = true;
	}

	void Renderer3D::EndScene()
	{
		if (!m_SceneActive)
			return; // guard against EndScene() without BeginScene() (or a double call)

		m_SceneActive = false;

		// Written into this frame's command list ahead of every draw that binds it
		// (CommandList::UploadBuffer, the same path material UBOs use).
		ResolveSceneLights();
		Renderer::Upload(m_SceneUniformBuffer, &m_CameraData, sizeof(CameraData));

		m_CameraData.AmbientColor = glm::vec4(0.0f);
		m_CameraData.LightCounts = glm::ivec4(0);
		m_LocalLights.clear();
		m_SceneLightSubmitted = false;
		m_DroppedLights = 0;

		const Renderer3DCapabilities& caps = m_Params.Capabilities;
		uint32_t batchIndex = 0;

		// One indexed draw per batch, each from its own pooled (vertex, index) buffer so no
		// shared buffer is re-uploaded between draws.
		for (Material* material : m_DrawOrder)
		{
			if (IsLitShader(material->GetShader()))
			{
				// The binding set would name bindings the shader's layout lacks, which Vulkan does
				// not reject: drawing it is undefined behaviour, so the material is skipped.
				if (BindsPastSlotZero(*material))
				{
					if (!m_LitSlotsWarned)
					{
						const std::string& name = material->GetParams().DebugName;
						DE_CORE_WARN("Renderer3D: lit material '{}' has a texture or sampler past slot 0, which the lit shader has no binding for; it is not drawn until that slot is cleared.",
							name.empty() ? "<unnamed>" : name.c_str());
						m_LitSlotsWarned = true;
					}
					continue;
				}
				PrepareLitMaterial(material);
			}

			MaterialBatch& matBatch = m_Batches[material];
			for (uint32_t chunkIndex = 0; chunkIndex < matBatch.ChunksInUse; ++chunkIndex)
			{
				MeshChunk& chunk = matBatch.Chunks[chunkIndex];
				if (chunk.Indices.empty())
					continue;

				// Grow the buffer pool on demand; each pooled pair holds a full-capacity batch.
				// DirectUpload = false: the writes go into the frame's command list (as
				// Renderer2D's batches do) instead of each spinning up a throwaway command list
				// and its own queue submit.
				if (batchIndex >= m_BatchVertexBuffers.size())
				{
					m_BatchVertexBuffers.push_back(GraphicsBuffer::CreateVertexBuffer(sizeof(Vertex) * caps.MaxVertices, nullptr, false, "Renderer3D_BatchVB"));
					m_BatchIndexBuffers.push_back(GraphicsBuffer::CreateIndexBuffer(sizeof(uint32_t) * caps.MaxIndices, nullptr, false, "Renderer3D_BatchIB", GraphicsFormat::Uint32));
				}

				GraphicsBuffer* vertexBuffer = m_BatchVertexBuffers[batchIndex];
				GraphicsBuffer* indexBuffer = m_BatchIndexBuffers[batchIndex];

				Renderer::Upload(vertexBuffer, chunk.Vertices.data(), static_cast<uint32_t>(chunk.Vertices.size() * sizeof(Vertex)));
				Renderer::Upload(indexBuffer, chunk.Indices.data(), static_cast<uint32_t>(chunk.Indices.size() * sizeof(uint32_t)));

				// Bind the shared camera/light UBO at binding 0 for this material, then draw. A custom
				// material also drawn skinned would otherwise keep a skin buffer this renderer may free.
				material->SetSceneUniformBuffer(m_SceneUniformBuffer);
				material->SetSkinUniformBuffer(nullptr);
				Renderer::DrawIndexed(material, m_Layout, vertexBuffer, indexBuffer, static_cast<uint32_t>(chunk.Indices.size()));
				++m_Statistics.DrawCalls;

				++batchIndex;
			}
		}

		DrawSkinnedSubmissions();
	}

	void Renderer3D::SubmitSkinnedMesh(const Mesh* mesh, const glm::mat4& transform, std::span<const glm::mat4> joints, const glm::vec4& color, Material* material)
	{
		if (!m_SceneActive || !mesh)
			return;

		const uint32_t jointCount = mesh->GetSkinJointCount();
		const char* fallbackReason = (!mesh->HasSkin() || mesh->GetIndices().empty()) ? "" :
			jointCount > k_MaxSkinJoints ? "uses more joints than one draw can skin" :
			joints.size() < jointCount ? "was given fewer joints than it uses" : nullptr;
		if (fallbackReason)
		{
			if (*fallbackReason && !m_SkinFallbackWarned)
			{
				DE_CORE_WARN("Renderer3D: a skinned mesh {} ({} joints, {} passed, at most {} a draw); it is drawn in its rest pose without skinning.",
					fallbackReason, jointCount, joints.size(), k_MaxSkinJoints);
				m_SkinFallbackWarned = true;
			}

			// A skinned shader can't draw the static vertex stream.
			const Shader* shader = material ? material->GetShader() : nullptr;
			const bool skinnedOnly = shader && shader->FindUniformBufferBinding(Material::k_SkinDataBlockName) >= 0;
			SubmitMesh(mesh, transform, color, skinnedOnly ? nullptr : material);
			return;
		}

		bool shared = false;
		if (!m_SkinnedInstances.empty())
		{
			const SkinnedInstance& last = m_SkinnedInstances.back();
			const uint32_t overlap = std::min(last.JointCount, jointCount);
			shared = last.Source == joints.data() && last.Transform == transform && last.Color == color &&
				std::memcmp(m_SkinnedJoints.data() + last.FirstJoint, joints.data(), overlap * sizeof(glm::mat4)) == 0;
		}
		if (!shared)
		{
			SkinnedInstance& instance = m_SkinnedInstances.emplace_back();
			instance.Source = joints.data();
			instance.Transform = transform;
			instance.Color = color;
			instance.FirstJoint = static_cast<uint32_t>(m_SkinnedJoints.size());
		}

		// The instance is the last one added, so its joints end the arena and can grow in place.
		SkinnedInstance& instance = m_SkinnedInstances.back();
		if (jointCount > instance.JointCount)
		{
			m_SkinnedJoints.insert(m_SkinnedJoints.end(), joints.begin() + instance.JointCount, joints.begin() + jointCount);
			instance.JointCount = jointCount;
		}

		m_SkinnedSubmissions.push_back({ mesh, material, static_cast<uint32_t>(m_SkinnedInstances.size() - 1) });
	}

	uint32_t Renderer3D::GetSkinnedInstanceBudget() const
	{
		return std::clamp(m_Params.Capabilities.MaxSkinnedInstances, 1u, k_MaxSkinnedInstancesLimit);
	}

	void Renderer3D::EnsureSkinningResources()
	{
		if (m_SkinnedShader)
			return;

		m_SkinnedShader = CreateLitShader(k_SkinnedLitShaderName, true);
		Internal::WatchUnmanagedShader(m_SkinnedShader);

		m_SkinBuffer = GraphicsBuffer::Create(GraphicsBufferParams()
			.SetDebugName("Renderer3D_SkinUBO")
			.SetByteSize(sizeof(SkinData))
			.SetType(BufferType::UniformBuffer)
			.SetIsVolatile(true)
			.SetDirectUpload(false)
			.SetMaxWritesPerFrame(GetSkinnedInstanceBudget()));

		m_FullSkinUploads = GraphicsContext::Get().GetParams().GraphicsAPI == GraphicsAPI::DirectX11;
	}

	Material* Renderer3D::ResolveSkinnedMaterial(Material* material)
	{
		Material* requested = material ? material : m_Material;
		if (IsLitShader(requested->GetShader()))
			return GetSkinnedTwin(requested);

		// The binding must not collide with the scene and material blocks or with a texture or
		// sampler the material binds, or its binding set fails and the mesh silently vanishes.
		const Shader* shader = requested->GetShader();
		const int32_t binding = shader ? shader->FindUniformBufferBinding(Material::k_SkinDataBlockName) : -1;
		bool usable = binding >= 2 && binding <= static_cast<int32_t>(k_MaxSkinDataBinding);
		if (usable)
		{
			const uint32_t slot = static_cast<uint32_t>(binding - 2) / 2;
			const bool samplerBinding = (binding - 2) % 2 == 1;
			usable = samplerBinding ? !requested->GetSampler(slot) : !requested->GetTexture(slot);
		}
		if (usable)
			return requested;

		if (!m_SkinMaterialWarned)
		{
			const std::string& name = requested->GetParams().DebugName;
			DE_CORE_WARN("Renderer3D: material '{}' has no SkinData uniform block at a free binding from 2 to {}, so skinned meshes drawn with it use the default lit material.",
				name.empty() ? "<unnamed>" : name.c_str(), k_MaxSkinDataBinding);
			m_SkinMaterialWarned = true;
		}
		return GetSkinnedTwin(m_Material);
	}

	Material* Renderer3D::GetSkinnedTwin(Material* source)
	{
		SkinnedTwin& entry = m_SkinnedTwins[source->GetId()];
		if (!entry.Twin)
		{
			MaterialParams twinParams = source->GetParams();
			twinParams.SetDebugName(twinParams.DebugName + "_Skinned").SetShader(m_SkinnedShader).SetCullMode(CullMode::None);
			entry.Twin = Material::Create(twinParams);
		}
		entry.Used = true;

		Material* twin = entry.Twin;
		if (entry.SourceRevision != source->GetBindingRevision())
		{
			// The source may have cleared a slot and refilled it with a texture at the freed one's
			// address. Clearing the twin's slot first makes it rebind instead of keeping the old one.
			twin->SetTexture(0, nullptr);
			twin->SetSampler(0, nullptr);
			entry.SourceRevision = source->GetBindingRevision();
		}

		// A slot's pointer changing rebuilds the twin's pipelines, so an empty source slot maps to
		// the same default PrepareLitMaterial would put there.
		twin->SetTexture(0, source->GetTexture(0) ? source->GetTexture(0) : Renderer::GetWhiteTexture());
		twin->SetSampler(0, source->GetSampler(0) ? source->GetSampler(0) : Renderer::GetClampSampler());
		twin->SetEmissiveColor(source->GetEmissiveColor());
		twin->SetEmissiveStrength(source->GetEmissiveStrength());
		twin->SetRoughness(source->GetRoughness());
		twin->SetSpecular(source->GetSpecular());
		PrepareLitMaterial(twin);
		return twin;
	}

	void Renderer3D::DrawSkinnedSubmissions()
	{
		if (m_SkinnedSubmissions.empty())
			return;

		EnsureSkinningResources();

		// The skin buffer's writes are a per-frame budget, shared by every scene in the frame.
		const uint64_t frameIndex = Renderer::GetFrameIndex();
		if (frameIndex != m_SkinnedFrameIndex)
		{
			m_SkinnedFrameIndex = frameIndex;
			m_SkinnedInstancesThisFrame = 0;
		}

		const uint32_t budget = GetSkinnedInstanceBudget();
		uint32_t uploadedInstance = ~0u;
		bool instanceDropped = false;
		for (const SkinnedSubmission& submission : m_SkinnedSubmissions)
		{
			// An instance is drawn whole or not at all, so a character never loses some of its parts.
			if (submission.Instance != uploadedInstance)
			{
				uploadedInstance = submission.Instance;
				instanceDropped = m_SkinnedInstancesThisFrame >= budget;
				if (instanceDropped)
				{
					DE_CORE_ASSERT(!m_Params.Capabilities.AssertOnOverflow,
						"Renderer3D: more skinned instances in a frame than MaxSkinnedInstances and AssertOnOverflow is set.");

					if (!m_SkinnedBudgetWarned)
					{
						DE_CORE_WARN("Renderer3D: more than {} skinned instances in one frame; the rest are skipped. Raise Renderer3DCapabilities.MaxSkinnedInstances (at most {}).",
							budget, k_MaxSkinnedInstancesLimit);
						m_SkinnedBudgetWarned = true;
					}
				}
				else
				{
					const SkinnedInstance& instance = m_SkinnedInstances[submission.Instance];
					m_SkinData.Model = instance.Transform;
					m_SkinData.NormalMatrix = glm::mat4(glm::inverseTranspose(glm::mat3(instance.Transform)));
					m_SkinData.Color = instance.Color;
					std::copy_n(m_SkinnedJoints.begin() + instance.FirstJoint, instance.JointCount, m_SkinData.Joints);
					const uint64_t uploadSize = m_FullSkinUploads ? sizeof(SkinData) : offsetof(SkinData, Joints) + instance.JointCount * sizeof(glm::mat4);
					Renderer::Upload(m_SkinBuffer, &m_SkinData, uploadSize);

					++m_SkinnedInstancesThisFrame;
					++m_Statistics.SkinnedInstances;
					m_Statistics.SkinnedJoints += instance.JointCount;
				}
			}

			if (instanceDropped)
			{
				++m_Statistics.DroppedSkinnedDraws;
				continue;
			}

			// The same rule as the static path, so a material draws the same way skinned or not.
			Material* requested = submission.Material ? submission.Material : m_Material;
			if (IsLitShader(requested->GetShader()) && BindsPastSlotZero(*requested))
			{
				if (!m_LitSlotsWarned)
				{
					const std::string& name = requested->GetParams().DebugName;
					DE_CORE_WARN("Renderer3D: lit material '{}' has a texture or sampler past slot 0, which the lit shader has no binding for; it is not drawn until that slot is cleared.",
						name.empty() ? "<unnamed>" : name.c_str());
					m_LitSlotsWarned = true;
				}
				continue;
			}

			Material* material = ResolveSkinnedMaterial(submission.Material);
			const Mesh* mesh = submission.Mesh;
			if (!mesh->m_SkinVertexBuffer)
			{
				const std::vector<SkinnedMeshVertex>& vertices = mesh->GetSkinVertices();
				const std::vector<uint32_t>& indices = mesh->GetIndices();
				mesh->m_SkinVertexBuffer = GraphicsBuffer::CreateVertexBuffer(vertices.size() * sizeof(SkinnedMeshVertex), nullptr, false, "Renderer3D_SkinVB");
				mesh->m_SkinIndexBuffer = GraphicsBuffer::CreateIndexBuffer(indices.size() * sizeof(uint32_t), nullptr, false, "Renderer3D_SkinIB", GraphicsFormat::Uint32);
				Renderer::Upload(mesh->m_SkinVertexBuffer, vertices.data(), vertices.size() * sizeof(SkinnedMeshVertex));
				Renderer::Upload(mesh->m_SkinIndexBuffer, indices.data(), indices.size() * sizeof(uint32_t));
			}

			material->SetSceneUniformBuffer(m_SceneUniformBuffer);
			material->SetSkinUniformBuffer(m_SkinBuffer);
			Renderer::DrawIndexed(material, m_SkinnedLayout, mesh->m_SkinVertexBuffer, mesh->m_SkinIndexBuffer, mesh->GetIndexCount());

			++m_Statistics.DrawCalls;
			++m_Statistics.SkinnedDraws;
			++m_Statistics.SubmittedMeshes;
		}

		m_SkinnedSubmissions.clear();
		m_SkinnedInstances.clear();
		m_SkinnedJoints.clear();
	}

	Material* Renderer3D::CreateLitMaterial(MaterialParams params) const
	{
		params.SetShader(m_Shader).SetCullMode(CullMode::None);
		Material* material = Material::Create(params);
		PrepareLitMaterial(material);
		return material;
	}

	void Renderer3D::PrepareLitMaterial(Material* material) const
	{
		// The lit shader always samples an albedo texture, and a material's binding set has to
		// match the shader's exactly.
		if (!material->GetTexture(0))
			material->SetTexture(0, Renderer::GetWhiteTexture());
		if (!material->GetSampler(0))
			material->SetSampler(0, Renderer::GetClampSampler());

		// Rebuilt every scene: SetRoughness and the other surface setters only change the
		// material's params, never its uniform data.
		LitMaterialData data;
		const glm::vec3& emissiveColor = material->GetEmissiveColor();
		data.EmissiveColor = glm::vec4(IsFinite(emissiveColor) ? emissiveColor : glm::vec3(0.0f), 0.0f);
		data.Surface = glm::vec4(
			FiniteOr(material->GetEmissiveStrength(), 0.0f),
			glm::clamp(FiniteOr(material->GetRoughness(), 0.5f), 0.0f, 1.0f),
			std::max(FiniteOr(material->GetSpecular(), 0.0f), 0.0f),
			0.0f);

		// SetUniform forces a re-upload, and every upload spends one of the buffer's writes for the
		// frame, so a material drawn in several scenes of a frame only rewrites what changed.
		const std::vector<uint8_t>& current = material->GetUniformCPUData();
		if (current.size() == sizeof(data) && std::memcmp(current.data(), &data, sizeof(data)) == 0)
			return;
		material->SetUniform(data);
	}

	void Renderer3D::Clear(const glm::vec4& clearColor)
	{
		Renderer::Clear(clearColor);
	}

	bool Renderer3D::SubmitLight(const DirectionalLight& light)
	{
		m_SceneLightSubmitted = true;
		if (!IsFinite(light.Direction) || !IsFinite(light.Color * light.Intensity))
			return false;

		int& count = m_CameraData.LightCounts.x;
		if (count >= static_cast<int>(k_MaxDirectionalLights))
		{
			DE_CORE_ASSERT(!m_Params.Capabilities.AssertOnOverflow,
				"Renderer3D: more directional lights than k_MaxDirectionalLights and AssertOnOverflow is set.");

			if (!m_DirectionalOverflowWarned)
			{
				DE_CORE_WARN("Renderer3D: a scene submitted more than {} directional lights; the extra ones are dropped.", k_MaxDirectionalLights);
				m_DirectionalOverflowWarned = true;
			}
			++m_DroppedLights;
			return false;
		}

		m_CameraData.DirectionalLights[count] = { glm::vec4(light.Direction, 0.0f), glm::vec4(light.Color * light.Intensity, 0.0f) };
		++count;
		return true;
	}

	template<typename LightType>
	bool Renderer3D::SubmitLocalLight(const LightType& light)
	{
		m_SceneLightSubmitted = true;
		if (!Internal::IsUsableLight(light))
			return false;

		LocalLightCandidate* candidate = AddLocalLight();
		if (!candidate)
			return false;

		const Internal::LightCone cone = Internal::GetLightCone(light);
		candidate->Data.PositionRange = glm::vec4(light.Position, light.Range);
		candidate->Data.Color = glm::vec4(light.Color * light.Intensity, cone.Scale);
		candidate->Data.SpotDirection = glm::vec4(cone.Axis, cone.Offset);
		candidate->Brightness = Strength(light.Color) * light.Intensity;
		return true;
	}

	bool Renderer3D::SubmitLight(const PointLight& light)
	{
		return SubmitLocalLight(light);
	}

	bool Renderer3D::SubmitLight(const SpotLight& light)
	{
		return SubmitLocalLight(light);
	}

	Renderer3D::LocalLightCandidate* Renderer3D::AddLocalLight()
	{
		// Pending lights only clear at EndScene, so code that submits lights every frame but never
		// ends a scene would otherwise grow this without bound.
		if (m_LocalLights.size() >= k_MaxPendingLocalLights)
		{
			if (!m_PendingOverflowWarned)
			{
				DE_CORE_WARN("Renderer3D: {} point and spot lights are waiting for an EndScene; further ones are ignored.", k_MaxPendingLocalLights);
				m_PendingOverflowWarned = true;
			}
			return nullptr;
		}
		return &m_LocalLights.emplace_back();
	}

	void Renderer3D::SetAmbientLight(const glm::vec3& color, float intensity)
	{
		m_SceneLightSubmitted = true;
		if (IsFinite(color * intensity))
			m_CameraData.AmbientColor = glm::vec4(color * intensity, 0.0f);
	}

	uint32_t Renderer3D::GetLocalLightBudget() const
	{
		return std::min(m_Params.Capabilities.MaxLocalLights, k_MaxLocalLights);
	}

	void Renderer3D::SetDirectionalLight(const glm::vec3& direction, float ambient)
	{
		m_Params.LightDirection = direction;
		m_Params.Ambient = ambient;
	}

	void Renderer3D::ResolveSceneLights()
	{
		if (!m_SceneLightSubmitted)
		{
			// The default light's ambient + (1 - ambient) * N·L, as one white light over a white
			// ambient.
			const float ambient = m_Params.Ambient;
			m_CameraData.AmbientColor = glm::vec4(glm::vec3(ambient), 0.0f);
			m_CameraData.DirectionalLights[0] = { glm::vec4(m_Params.LightDirection, 0.0f), glm::vec4(glm::vec3(1.0f - ambient), 0.0f) };
			m_CameraData.LightCounts.x = 1;
		}

		const int directionalCount = m_CameraData.LightCounts.x;
		m_CameraData.LightDirection = directionalCount > 0 ? m_CameraData.DirectionalLights[0].Direction : glm::vec4(m_Params.LightDirection, 0.0f);
		const glm::vec4& ambientColor = m_CameraData.AmbientColor;
		m_CameraData.Ambient = glm::vec4(std::max({ ambientColor.r, ambientColor.g, ambientColor.b }), 0.0f, 0.0f, 0.0f);

		const FrustumPlanes planes = ExtractFrustumPlanes(m_CameraData.ViewProjection);
		const glm::vec4& camera = m_CameraData.CameraPosition;

		m_VisibleLocalLights.clear();
		for (uint32_t index = 0; index < m_LocalLights.size(); ++index)
		{
			LocalLightCandidate& candidate = m_LocalLights[index];
			const glm::vec3 position = glm::vec3(candidate.Data.PositionRange);
			const float range = candidate.Data.PositionRange.w;
			if (!SphereTouchesFrustum(planes, position, range))
				continue;

			// How bright the light looks from the camera: full strength while the camera is inside
			// its range, then falling with the distance from the range's edge. Every light whose
			// range holds the camera scores the same, so Nearness breaks those ties.
			const float distance = camera.w > 0.0f ? glm::distance(glm::vec3(camera), position) : 0.0f;
			const float gap = std::max(distance - range, 0.0f);
			candidate.Score = candidate.Brightness / (1.0f + gap * gap);
			candidate.Nearness = distance / range;
			m_VisibleLocalLights.push_back(index);
		}

		const uint32_t visibleCount = static_cast<uint32_t>(m_VisibleLocalLights.size());
		const uint32_t budget = GetLocalLightBudget();
		uint32_t localCount = visibleCount;
		if (visibleCount > budget)
		{
			DE_CORE_ASSERT(!m_Params.Capabilities.AssertOnOverflow,
				"Renderer3D: more point and spot lights in view than MaxLocalLights and AssertOnOverflow is set.");

			std::partial_sort(m_VisibleLocalLights.begin(), m_VisibleLocalLights.begin() + budget, m_VisibleLocalLights.end(),
				[this](uint32_t a, uint32_t b)
				{
					const LocalLightCandidate& lightA = m_LocalLights[a];
					const LocalLightCandidate& lightB = m_LocalLights[b];
					if (lightA.Score != lightB.Score)
						return lightA.Score > lightB.Score;
					if (lightA.Nearness != lightB.Nearness)
						return lightA.Nearness < lightB.Nearness;
					return a < b;
				});

			if (!m_LocalOverflowWarned)
			{
				DE_CORE_WARN("Renderer3D: {} point and spot lights reach the view but MaxLocalLights is {}; the dimmest are dropped. Raise Renderer3DCapabilities.MaxLocalLights (at most {}) or use fewer lights.",
					visibleCount, budget, k_MaxLocalLights);
				m_LocalOverflowWarned = true;
			}

			m_DroppedLights += visibleCount - budget;
			localCount = budget;
		}

		for (uint32_t slot = 0; slot < localCount; ++slot)
			m_CameraData.LocalLights[slot] = m_LocalLights[m_VisibleLocalLights[slot]].Data;
		m_CameraData.LightCounts.y = static_cast<int>(localCount);

		m_Statistics.DirectionalLights = static_cast<uint32_t>(directionalCount);
		m_Statistics.LocalLights = localCount;
		m_Statistics.CulledLights = static_cast<uint32_t>(m_LocalLights.size()) - visibleCount;
		m_Statistics.DroppedLights = m_DroppedLights;
	}

	void Renderer3D::SubmitMesh(const Mesh* mesh, const glm::mat4& transform, const glm::vec4& color, Material* material)
	{
		if (!m_SceneActive || !mesh)
			return;

		Material* batchMaterial = material ? material : m_Material;
		MaterialBatch& matBatch = m_Batches[batchMaterial];
		if (!matBatch.Enqueued)
		{
			matBatch.Enqueued = true;
			m_DrawOrder.push_back(batchMaterial);
		}

		const std::vector<MeshVertex>& vertices = mesh->GetVertices();
		const std::vector<uint32_t>& indices = mesh->GetIndices();
		const Renderer3DCapabilities& caps = m_Params.Capabilities;

		// A mesh bigger than an empty batch fits no batch at all, so spilling cannot help.
		if (vertices.size() > caps.MaxVertices || indices.size() > caps.MaxIndices)
		{
			// DE_CORE_ASSERT takes a plain message, not a format string — the vert/index
			// counts are in the warn below.
			DE_CORE_ASSERT(!caps.AssertOnOverflow,
				"Renderer3D mesh exceeds a single batch's capacity and AssertOnOverflow is set. Raise Renderer3DCapabilities or submit a smaller mesh.");

			if (!m_MeshOverflowWarned)
			{
				DE_CORE_WARN("Renderer3D mesh exceeds a single batch's capacity ({} verts / {} indices); dropping this mesh. Raise Renderer3DCapabilities.MaxVertices/MaxIndices.",
					caps.MaxVertices, caps.MaxIndices);
				m_MeshOverflowWarned = true;
			}
			++m_Statistics.DroppedMeshes;
			return;
		}

		if (matBatch.ChunksInUse == 0)
		{
			if (matBatch.Chunks.empty())
				matBatch.Chunks.emplace_back();
			matBatch.ChunksInUse = 1;
		}

		MeshChunk* chunk = &matBatch.Chunks[matBatch.ChunksInUse - 1];
		if (chunk->Vertices.size() + vertices.size() > caps.MaxVertices ||
			chunk->Indices.size() + indices.size() > caps.MaxIndices)
		{
			if (matBatch.ChunksInUse == matBatch.Chunks.size())
				matBatch.Chunks.emplace_back();
			chunk = &matBatch.Chunks[matBatch.ChunksInUse];
			++matBatch.ChunksInUse;
		}

		// Normals need the inverse-transpose so non-uniform scale (stretched walls)
		// doesn't skew them; positions just use the model matrix.
		const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(transform));
		const uint32_t vertexOffset = static_cast<uint32_t>(chunk->Vertices.size());

		for (const MeshVertex& v : vertices)
		{
			Vertex vertex;
			vertex.Position = glm::vec3(transform * glm::vec4(v.Position, 1.0f));
			vertex.Normal = normalMatrix * v.Normal;
			vertex.Color = color;
			vertex.TexCoord = v.TexCoord;
			chunk->Vertices.push_back(vertex);
		}

		for (uint32_t index : indices)
			chunk->Indices.push_back(index + vertexOffset);

		++m_Statistics.SubmittedMeshes;
		m_Statistics.VertexCount += static_cast<uint32_t>(vertices.size());
		m_Statistics.IndexCount += static_cast<uint32_t>(indices.size());
	}

	void Renderer3D::DrawBox(const glm::mat4& transform, const glm::vec4& color)
	{
		SubmitMesh(m_BoxMesh, transform, color);
	}

	void Renderer3D::DrawSphere(const glm::mat4& transform, const glm::vec4& color)
	{
		SubmitMesh(m_SphereMesh, transform, color);
	}

}
