#include "depch.h"
#include "DingoEngine/Graphics/Renderer3D.h"
#include "DingoEngine/Asset/UnmanagedShaderWatch.h"
#include "DingoEngine/Graphics/EngineShaders.h"
#include "DingoEngine/Graphics/FullscreenPass.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/LightMath.h"
#include "DingoEngine/Graphics/ParticleRenderer.h"
#include "DingoEngine/Graphics/PostProcess.h"
#include "DingoEngine/Graphics/TextureReadback.h"

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstring>
#include <type_traits>

namespace
{
	constexpr const char* k_LitShaderName = "Renderer3DMeshShader";
	constexpr const char* k_SkinnedLitShaderName = "Renderer3DSkinnedMeshShader";

	Dingo::Shader* CreateLitShader(const char* name, bool skinned)
	{
		Dingo::ShaderParams params = Dingo::ShaderParams().SetName(name);
		if (skinned)
			params.AddDefine("DE_SKINNED");
		return Dingo::Internal::CreateEngineShader(params, "Renderer3D_Lit.glsl");
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

	uint32_t FloorPowerOfTwo(uint32_t value)
	{
		uint32_t power = 1;
		while (power * 2 <= value)
			power *= 2;
		return power;
	}

	// 0 at the cutoff, 1 from (1 + band) times it up, smooth between.
	float BudgetFade(float priority, float cutoff, float band)
	{
		if (!(cutoff > 0.0f))
			return 1.0f;
		const float t = std::clamp((priority / cutoff - 1.0f) / band, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	bool SphereTouchesBox(const glm::vec3& center, float radius, const glm::vec3& low, const glm::vec3& high)
	{
		const glm::vec3 nearest = glm::clamp(center, low, high);
		const glm::vec3 offset = nearest - center;
		return glm::dot(offset, offset) <= radius * radius;
	}

	// A tile wider than its cone by two texels on each side, so the 3 x 3 PCF kernel near the cone's
	// edge (or a cube face's) reads depth that was rendered.
	float TileHalfAngleTangent(float coneTangent, uint32_t size)
	{
		return coneTangent * static_cast<float>(size) / static_cast<float>(size - 4);
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

	// Square power-of-two tiles of a square atlas: each takes the smallest free square that holds it,
	// split in four until it fits.
	class Renderer3D::ShadowAtlasAllocator
	{
	public:
		explicit ShadowAtlasAllocator(uint32_t size) { m_Free.push_back({ 0, 0, size }); }

		bool Allocate(uint32_t size, glm::uvec2& corner)
		{
			int best = -1;
			for (int i = 0; i < static_cast<int>(m_Free.size()); ++i)
			{
				if (m_Free[i].Size >= size && (best < 0 || m_Free[i].Size < m_Free[best].Size))
					best = i;
			}
			if (best < 0)
				return false;

			Square square = m_Free[best];
			m_Free.erase(m_Free.begin() + best);
			while (square.Size > size)
			{
				const uint32_t half = square.Size / 2;
				m_Free.push_back({ square.X + half, square.Y, half });
				m_Free.push_back({ square.X, square.Y + half, half });
				m_Free.push_back({ square.X + half, square.Y + half, half });
				square.Size = half;
			}
			corner = { square.X, square.Y };
			return true;
		}

	private:
		struct Square
		{
			uint32_t X = 0;
			uint32_t Y = 0;
			uint32_t Size = 0;
		};
		std::vector<Square> m_Free;
	};

	Renderer3D* Renderer3D::Create(const Renderer3DParams& params)
	{
		Renderer3D* renderer = new Renderer3D(params);
		renderer->Initialize();
		return renderer;
	}

	Renderer3D::Renderer3D(const Renderer3DParams& params)
		: m_Params(params)
	{
	}

	Renderer3D::~Renderer3D()
	{
		std::erase(s_LitShaders, m_Shader);
		Internal::UnwatchUnmanagedShader(m_Shader);
		Internal::UnwatchUnmanagedShader(m_SkinnedShader);
		Internal::UnwatchUnmanagedShader(m_ShadowShader);
		Internal::UnwatchUnmanagedShader(m_SkinnedShadowShader);
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

		// Vulkan takes locations from attribute order and warns about every one the vertex shader
		// skips, so the depth pass reads only what Renderer3D_Shadow.glsl declares.
		m_SkinnedShadowLayout = VertexLayout()
			.SetStride(sizeof(SkinnedMeshVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(SkinnedMeshVertex, Position))
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

		// The lit shader binds the shadows always, so every scene writes them, cast or not.
		m_ShadowDataBuffer = GraphicsBuffer::Create(GraphicsBufferParams()
			.SetDebugName("Renderer3D_ShadowData")
			.SetByteSize(sizeof(ShadowData))
			.SetType(BufferType::UniformBuffer)
			.SetIsVolatile(true)
			.SetDirectUpload(false)
			.SetMaxWritesPerFrame(k_MaxScenesPerFrame));
		m_ShadowSampler = Sampler::Create(SamplerParams().SetCompare(true));
		m_PlaceholderShadowAtlas = Texture::Create(TextureParams()
			.SetDebugName("Renderer3D_NoShadowAtlas")
			.SetWidth(1)
			.SetHeight(1)
			.SetFormat(TextureFormat::D32)
			.SetDimension(TextureDimension::Texture2D)
			.SetIsRenderTarget(true));
		SetShadowSettings(m_Params.Shadows);

		m_Particles = std::make_unique<Internal::ParticleRenderer>(std::clamp(m_Params.Capabilities.MaxParticles, 64u, k_MaxParticlesLimit));

		// Built-in unit primitives for the DrawBox/DrawSphere conveniences.
		m_BoxMesh = Mesh::CreateBox();
		m_SphereMesh = Mesh::CreateSphere(0.5f, 16, 16);
	}

	void Renderer3D::Shutdown()
	{
		m_Particles.reset();

		for (GraphicsBuffer*& buffer : m_BatchVertexBuffers)
			DestroyAndDelete(buffer);
		for (GraphicsBuffer*& buffer : m_BatchIndexBuffers)
			DestroyAndDelete(buffer);
		m_BatchVertexBuffers.clear();
		m_BatchIndexBuffers.clear();
		m_Batches.clear();
		m_DrawOrder.clear();
		m_ChunkDraws.clear();
		m_TranslucentSubmissions.clear();
		m_TranslucentVertices.clear();
		m_TranslucentIndices.clear();
		m_TranslucentChunks.clear();
		m_TranslucentDraws.clear();
		m_TranslucentMaterials.clear();

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

		DestroyShadowMaterials();
		Internal::UnwatchUnmanagedShader(m_ShadowShader);
		Internal::UnwatchUnmanagedShader(m_SkinnedShadowShader);
		DestroyAndDelete(m_ShadowShader);
		DestroyAndDelete(m_SkinnedShadowShader);
		DestroyAndDelete(m_ShadowAtlas);
		DestroyAndDelete(m_PlaceholderShadowAtlas);
		DestroyAndDelete(m_ShadowSampler);
		DestroyAndDelete(m_ShadowDataBuffer);
		DestroyAndDelete(m_ShadowViewsBuffer);

		DestroyAndDelete(m_ProbeMaterial);
		delete m_ProbeShader;
		m_ProbeShader = nullptr;
		DestroyAndDelete(m_ProbeBuffer);
		DestroyAndDelete(m_ProbeTarget);
		m_ProbeReadbacks.clear();
		m_ShadowProbes.clear();
		m_GpuProbeKeys.clear();

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
		m_SceneActive = true;
		m_SceneSkipped = Renderer::IsFrameSkipped();
		if (m_SceneSkipped)
			return;

		m_CameraData.ViewProjection = viewProjection;
		m_CameraData.CameraPosition = cameraPosition;

		const uint64_t frame = Renderer::GetFrameIndex();
		m_SceneOfFrame = frame == m_SceneFrame ? m_SceneOfFrame + 1 : 0;
		m_SceneFrame = frame;

		m_Statistics = {};
		m_HasCasters = false;
		m_StaticCasters = 0;

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
		m_TranslucentSubmissions.clear();
		m_TranslucentVertices.clear();
		m_TranslucentIndices.clear();
	}

	void Renderer3D::EndScene()
	{
		if (!m_SceneActive)
			return; // guard against EndScene() without BeginScene() (or a double call)

		m_SceneActive = false;
		if (m_SceneSkipped)
		{
			ClearSceneLights();
			if (m_Particles)
				m_Particles->ClearScene();
			return;
		}

		DE_PROFILE_SCOPE("Renderer3D::EndScene");
		Renderer::BeginGpuTimer("Renderer3D");

		// Written into this frame's command list ahead of every draw that binds it
		// (CommandList::UploadBuffer, the same path material UBOs use).
		{
			DE_PROFILE_SCOPE("Renderer3D::ResolveSceneLights");
			ResolveSceneLights();
		}
		m_Statistics.Fogged = m_CameraData.FogParams.w > 0.5f && m_CameraData.CameraPosition.w > 0.5f;
		Renderer::Upload(m_SceneUniformBuffer, &m_CameraData, sizeof(CameraData));
		const bool shadows = PrepareShadows();
		Renderer::Upload(m_ShadowDataBuffer, &m_ShadowData, sizeof(ShadowData));
		ResolveShadowProbes();
		ClearSceneLights();

		uint32_t batchIndex = 0;

		// Every chunk goes into its own pooled (vertex, index) buffer up front, so no shared buffer
		// is re-uploaded between draws and the shadow pass can draw them before the lit pass does.
		{
			DE_PROFILE_SCOPE("Renderer3D::UploadBatches");
			m_ChunkDraws.clear();
			for (const BatchKey& key : m_DrawOrder)
			{
				Material* material = key.Material;
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

				MaterialBatch& matBatch = m_Batches[key];
				for (uint32_t chunkIndex = 0; chunkIndex < matBatch.ChunksInUse; ++chunkIndex)
				{
					MeshChunk& chunk = matBatch.Chunks[chunkIndex];
					if (chunk.Indices.empty())
						continue;

					const uint32_t buffer = UploadChunk(chunk, batchIndex);
					m_ChunkDraws.push_back({ key, buffer, static_cast<uint32_t>(chunk.Indices.size()) });
				}
			}
			PrepareTranslucentPass(batchIndex, shadows);
		}

		ResolveSkinnedBudget();
		if (shadows)
		{
			DrawShadowPass();
			DrawShadowProbes();
		}

		Texture* shadowAtlas = m_ShadowAtlas ? m_ShadowAtlas->GetDepthAttachment() : m_PlaceholderShadowAtlas;
		{
			DE_PROFILE_SCOPE("Renderer3D::DrawBatches");
			for (const ChunkDraw& draw : m_ChunkDraws)
			{
				if (draw.Translucent || draw.Key.Shadows == ShadowCasting::ShadowsOnly)
					continue;

				// Bind the shared camera/light UBO at binding 0 for this material, then draw. A custom
				// material also drawn skinned would otherwise keep a skin buffer this renderer may free.
				Material* material = draw.Key.Material;
				material->SetSceneUniformBuffer(m_SceneUniformBuffer);
				material->SetSkinUniformBuffer(nullptr);
				material->SetShadowResources(m_ShadowDataBuffer, shadowAtlas, m_ShadowSampler);
				Renderer::DrawIndexed(material, m_Layout, m_BatchVertexBuffers[draw.Buffer], m_BatchIndexBuffers[draw.Buffer], draw.IndexCount);
				++m_Statistics.DrawCalls;
			}
		}

		DrawSkinnedSubmissions();
		DrawTranslucentPass(shadowAtlas);
		m_SkinnedSubmissions.clear();
		m_SkinnedInstances.clear();
		m_SkinnedJoints.clear();
		m_TranslucentSubmissions.clear();
		m_TranslucentVertices.clear();
		m_TranslucentIndices.clear();

		if (m_Particles)
			m_Particles->EndScene(m_CameraData.ViewProjection, m_Statistics);
		Renderer::EndGpuTimer();
	}

	uint32_t Renderer3D::UploadChunk(const MeshChunk& chunk, uint32_t& batchIndex)
	{
		// Grow the buffer pool on demand; each pooled pair holds a full-capacity batch.
		// DirectUpload = false: the writes go into the frame's command list (as Renderer2D's
		// batches do) instead of each spinning up a throwaway command list and its own queue submit.
		const Renderer3DCapabilities& caps = m_Params.Capabilities;
		if (batchIndex >= m_BatchVertexBuffers.size())
		{
			m_BatchVertexBuffers.push_back(GraphicsBuffer::CreateVertexBuffer(sizeof(Vertex) * caps.MaxVertices, nullptr, false, "Renderer3D_BatchVB"));
			m_BatchIndexBuffers.push_back(GraphicsBuffer::CreateIndexBuffer(sizeof(uint32_t) * caps.MaxIndices, nullptr, false, "Renderer3D_BatchIB", GraphicsFormat::Uint32));
		}

		Renderer::Upload(m_BatchVertexBuffers[batchIndex], chunk.Vertices.data(), static_cast<uint32_t>(chunk.Vertices.size() * sizeof(Vertex)));
		Renderer::Upload(m_BatchIndexBuffers[batchIndex], chunk.Indices.data(), static_cast<uint32_t>(chunk.Indices.size() * sizeof(uint32_t)));
		return batchIndex++;
	}

	float Renderer3D::TranslucentSortKey(const glm::vec3& point) const
	{
		const glm::vec4& camera = m_CameraData.CameraPosition;
		const glm::vec3 offset = point - glm::vec3(camera);
		const float key = camera.w > 0.5f ? glm::dot(offset, offset) : -glm::dot(point, glm::vec3(camera));
		return std::isfinite(key) ? key : 0.0f;
	}

	void Renderer3D::PrepareTranslucentPass(uint32_t& batchIndex, bool shadows)
	{
		m_TranslucentDraws.clear();
		m_TranslucentMaterials.clear();
		if (m_TranslucentSubmissions.empty())
			return;

		DE_PROFILE_SCOPE("Renderer3D::PrepareTranslucentPass");
		std::stable_sort(m_TranslucentSubmissions.begin(), m_TranslucentSubmissions.end(),
			[](const TranslucentSubmission& a, const TranslucentSubmission& b) { return a.SortKey > b.SortKey; });

		// The opaque path's rule for lit materials, checked once a material.
		auto drawable = [this](Material* material)
		{
			if (!IsLitShader(material->GetShader()))
				return true;
			for (const auto& [checked, usable] : m_TranslucentMaterials)
			{
				if (checked == material)
					return usable;
			}

			const bool usable = !BindsPastSlotZero(*material);
			if (usable)
				PrepareLitMaterial(material);
			else if (!m_LitSlotsWarned)
			{
				const std::string& name = material->GetParams().DebugName;
				DE_CORE_WARN("Renderer3D: lit material '{}' has a texture or sampler past slot 0, which the lit shader has no binding for; it is not drawn until that slot is cleared.",
					name.empty() ? "<unnamed>" : name.c_str());
				m_LitSlotsWarned = true;
			}
			m_TranslucentMaterials.emplace_back(material, usable);
			return usable;
		};

		const Renderer3DCapabilities& caps = m_Params.Capabilities;
		uint32_t chunksInUse = 0;
		auto startChunk = [this, &chunksInUse]()
		{
			if (chunksInUse == m_TranslucentChunks.size())
				m_TranslucentChunks.emplace_back();
			MeshChunk& chunk = m_TranslucentChunks[chunksInUse++];
			chunk.Vertices.clear();
			chunk.Indices.clear();
		};
		// Returns the chunk the mesh landed in and where its indices start there.
		auto append = [&](const TranslucentSubmission& submission, uint32_t& firstIndex)
		{
			MeshChunk* chunk = &m_TranslucentChunks[chunksInUse - 1];
			if (chunk->Vertices.size() + submission.VertexCount > caps.MaxVertices || chunk->Indices.size() + submission.IndexCount > caps.MaxIndices)
			{
				startChunk();
				chunk = &m_TranslucentChunks[chunksInUse - 1];
			}

			const uint32_t vertexOffset = static_cast<uint32_t>(chunk->Vertices.size());
			firstIndex = static_cast<uint32_t>(chunk->Indices.size());
			const auto vertices = m_TranslucentVertices.begin() + submission.FirstVertex;
			chunk->Vertices.insert(chunk->Vertices.end(), vertices, vertices + submission.VertexCount);
			for (uint32_t i = 0; i < submission.IndexCount; ++i)
				chunk->Indices.push_back(m_TranslucentIndices[submission.FirstIndex + i] + vertexOffset);
			return chunksInUse - 1;
		};

		// Depth-only, so the casters need no order: chunks apart from the lit ones, one run of chunks
		// per caster mask, since each mask draws with its own shadow material.
		struct CasterRun
		{
			uint32_t ShadowGroups = 1;
			uint32_t FirstChunk = 0;
			uint32_t ChunkEnd = 0;
		};
		std::vector<CasterRun> casterRuns;
		if (shadows)
		{
			std::vector<uint32_t> groups;
			for (const TranslucentSubmission& submission : m_TranslucentSubmissions)
			{
				if (submission.Skinned < 0 && submission.Shadows == ShadowCasting::On && submission.IndexCount > 0 &&
					std::find(groups.begin(), groups.end(), submission.ShadowGroups) == groups.end())
					groups.push_back(submission.ShadowGroups);
			}
			for (const uint32_t group : groups)
			{
				CasterRun run;
				run.ShadowGroups = group;
				for (const TranslucentSubmission& submission : m_TranslucentSubmissions)
				{
					if (submission.Skinned >= 0 || submission.Shadows != ShadowCasting::On || submission.ShadowGroups != group ||
						submission.IndexCount == 0 || !drawable(submission.Material))
						continue;
					if (run.ChunkEnd == 0)
					{
						startChunk();
						run.FirstChunk = chunksInUse - 1;
					}
					uint32_t firstIndex = 0;
					append(submission, firstIndex);
					run.ChunkEnd = chunksInUse;
				}
				if (run.ChunkEnd > 0)
					casterRuns.push_back(run);
			}
		}

		bool litChunkStarted = false;
		for (uint32_t index = 0; index < m_TranslucentSubmissions.size(); ++index)
		{
			const TranslucentSubmission& submission = m_TranslucentSubmissions[index];
			if (submission.Skinned >= 0)
			{
				m_TranslucentDraws.push_back({ submission.Material, submission.Skinned, 0, 0, 0, 1 });
				continue;
			}
			if (submission.IndexCount == 0 || !drawable(submission.Material))
				continue;

			if (!litChunkStarted)
			{
				startChunk();
				litChunkStarted = true;
			}
			uint32_t firstIndex = 0;
			const uint32_t chunk = append(submission, firstIndex);
			if (!m_TranslucentDraws.empty())
			{
				TranslucentDraw& last = m_TranslucentDraws.back();
				if (last.Skinned < 0 && last.Material == submission.Material && last.Buffer == chunk && last.FirstIndex + last.IndexCount == firstIndex)
				{
					last.IndexCount += submission.IndexCount;
					++last.Meshes;
					continue;
				}
			}
			m_TranslucentDraws.push_back({ submission.Material, -1, chunk, firstIndex, submission.IndexCount, 1 });
		}

		// Chunk indices become pooled buffer indices.
		std::vector<uint32_t> buffers(chunksInUse, 0);
		for (uint32_t chunk = 0; chunk < chunksInUse; ++chunk)
		{
			if (!m_TranslucentChunks[chunk].Indices.empty())
				buffers[chunk] = UploadChunk(m_TranslucentChunks[chunk], batchIndex);
		}
		for (const CasterRun& run : casterRuns)
		{
			for (uint32_t chunk = run.FirstChunk; chunk < run.ChunkEnd; ++chunk)
			{
				const MeshChunk& casters = m_TranslucentChunks[chunk];
				if (!casters.Indices.empty())
					m_ChunkDraws.push_back({ BatchKey{ nullptr, ShadowCasting::On, run.ShadowGroups }, buffers[chunk], static_cast<uint32_t>(casters.Indices.size()), true });
			}
		}
		for (TranslucentDraw& draw : m_TranslucentDraws)
		{
			if (draw.Skinned < 0)
				draw.Buffer = buffers[draw.Buffer];
		}
	}

	void Renderer3D::DrawTranslucentPass(Texture* shadowAtlas)
	{
		if (m_TranslucentDraws.empty())
			return;

		DE_PROFILE_SCOPE("Renderer3D::DrawTranslucentPass");
		// AO from the opaque depth behind them would darken them, as it would particles.
		PostProcessStack& post = Renderer::GetPostProcessStack();
		if (post.IsActive() && post.GetSceneTarget() == Renderer::GetRenderTarget())
			post.ApplyAmbientOcclusion();

		uint32_t uploadedInstance = ~0u;
		for (const TranslucentDraw& draw : m_TranslucentDraws)
		{
			if (draw.Skinned >= 0)
			{
				if (DrawSkinnedSubmission(m_SkinnedSubmissions[draw.Skinned], uploadedInstance, shadowAtlas))
				{
					++m_Statistics.TranslucentMeshes;
					++m_Statistics.TranslucentDraws;
				}
				continue;
			}

			Material* material = draw.Material;
			material->SetSceneUniformBuffer(m_SceneUniformBuffer);
			material->SetSkinUniformBuffer(nullptr);
			material->SetShadowResources(m_ShadowDataBuffer, shadowAtlas, m_ShadowSampler);
			Renderer::DrawIndexed(material, m_Layout, m_BatchVertexBuffers[draw.Buffer], m_BatchIndexBuffers[draw.Buffer], draw.IndexCount, 1, draw.FirstIndex);
			++m_Statistics.DrawCalls;
			++m_Statistics.TranslucentDraws;
			m_Statistics.TranslucentMeshes += draw.Meshes;
		}
		m_TranslucentDraws.clear();
	}

	std::shared_ptr<ParticleEmitter> Renderer3D::CreateParticleEmitter(const ParticleEffect* effect)
	{
		DE_CORE_ASSERT(m_Particles, "Renderer3D::CreateParticleEmitter after Shutdown.");
		return m_Particles->CreateEmitter(effect);
	}

	bool Renderer3D::OwnsParticleEmitter(const ParticleEmitter& emitter) const
	{
		return m_Particles && m_Particles->Owns(emitter);
	}

	void Renderer3D::SubmitParticles(ParticleEmitter& emitter, const glm::mat4& transform, float deltaTime)
	{
		if (!m_SceneActive || m_SceneSkipped || !m_Particles)
			return;
		m_Particles->Submit(emitter, transform, deltaTime);
	}

	GraphicsBuffer* Renderer3D::GetParticlePool() const
	{
		return m_Particles ? m_Particles->GetPool() : nullptr;
	}

	uint32_t Renderer3D::GetParticlePoolCapacity() const
	{
		return m_Particles ? m_Particles->GetCapacity() : 0;
	}

	uint32_t Renderer3D::GetParticlePoolUsed() const
	{
		return m_Particles ? m_Particles->GetUsed() : 0;
	}

	void Renderer3D::SubmitSkinnedMesh(const Mesh* mesh, const glm::mat4& transform, std::span<const glm::mat4> joints, const glm::vec4& color, Material* material, ShadowCasting shadows, uint32_t shadowGroups)
	{
		if (!m_SceneActive || m_SceneSkipped || !mesh)
			return;
		if (shadows == ShadowCasting::Off)
			shadowGroups = 1;

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
			SubmitMesh(mesh, transform, color, skinnedOnly ? nullptr : material, shadows, shadowGroups);
			return;
		}

		// The rest pose's box bounds the caster, padded by half its size for the animation.
		if (shadows != ShadowCasting::Off && mesh->GetBoundsMin().x <= mesh->GetBoundsMax().x)
		{
			glm::vec3 low(std::numeric_limits<float>::max());
			glm::vec3 high(std::numeric_limits<float>::lowest());
			for (int corner = 0; corner < 8; ++corner)
			{
				const glm::vec3 local((corner & 1) ? mesh->GetBoundsMax().x : mesh->GetBoundsMin().x,
					(corner & 2) ? mesh->GetBoundsMax().y : mesh->GetBoundsMin().y,
					(corner & 4) ? mesh->GetBoundsMax().z : mesh->GetBoundsMin().z);
				const glm::vec3 world = glm::vec3(transform * glm::vec4(local, 1.0f));
				low = glm::min(low, world);
				high = glm::max(high, world);
			}
			if (low.x <= high.x)
			{
				const glm::vec3 pad = (high - low) * 0.5f;
				m_CasterMin = m_HasCasters ? glm::min(m_CasterMin, low - pad) : low - pad;
				m_CasterMax = m_HasCasters ? glm::max(m_CasterMax, high + pad) : high + pad;
				m_HasCasters = true;
			}
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

		m_SkinnedSubmissions.push_back({ mesh, material, static_cast<uint32_t>(m_SkinnedInstances.size() - 1), shadows, shadowGroups });

		const Material* requested = material ? material : m_Material;
		if (requested->IsTranslucent() && shadows != ShadowCasting::ShadowsOnly)
		{
			m_SkinnedSubmissions.back().Translucent = true;

			// One key for an instance's translucent meshes keeps them adjacent through the stable sort,
			// so the pass uploads its palette once and the skin buffer's writes stay within budget.
			const uint32_t instanceIndex = static_cast<uint32_t>(m_SkinnedInstances.size() - 1);
			std::optional<float> sortKey;
			for (auto it = m_TranslucentSubmissions.rbegin(); it != m_TranslucentSubmissions.rend(); ++it)
			{
				if (it->Skinned < 0)
					continue;
				if (m_SkinnedSubmissions[it->Skinned].Instance == instanceIndex)
					sortKey = it->SortKey;
				break;
			}
			if (!sortKey)
			{
				glm::vec3 center = glm::vec3(transform[3]);
				if (mesh->GetBoundsMin().x <= mesh->GetBoundsMax().x)
					center = glm::vec3(transform * glm::vec4((mesh->GetBoundsMin() + mesh->GetBoundsMax()) * 0.5f, 1.0f));
				sortKey = TranslucentSortKey(center);
			}

			TranslucentSubmission& submission = m_TranslucentSubmissions.emplace_back();
			submission.Material = material;
			submission.Skinned = static_cast<int32_t>(m_SkinnedSubmissions.size() - 1);
			submission.Shadows = shadows;
			submission.ShadowGroups = shadowGroups;
			submission.SortKey = *sortKey;
		}
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
			.SetMaxWritesPerFrame(3 * GetSkinnedInstanceBudget()));

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
			// The source may have cleared a slot and refilled it with a sampler at the freed one's
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

	void Renderer3D::ResolveSkinnedBudget()
	{
		m_SkinnedInstanceDropped.assign(m_SkinnedInstances.size(), 0);
		if (m_SkinnedSubmissions.empty())
			return;

		// The skin buffer's writes are a per-frame budget, shared by every scene in the frame.
		const uint64_t frameIndex = Renderer::GetFrameIndex();
		if (frameIndex != m_SkinnedFrameIndex)
		{
			m_SkinnedFrameIndex = frameIndex;
			m_SkinnedInstancesThisFrame = 0;
		}

		// An instance is drawn whole or not at all, so a character never loses some of its parts.
		const uint32_t budget = GetSkinnedInstanceBudget();
		for (uint32_t instance = 0; instance < m_SkinnedInstances.size(); ++instance)
		{
			if (m_SkinnedInstancesThisFrame < budget)
			{
				++m_SkinnedInstancesThisFrame;
				++m_Statistics.SkinnedInstances;
				m_Statistics.SkinnedJoints += m_SkinnedInstances[instance].JointCount;
				continue;
			}

			m_SkinnedInstanceDropped[instance] = 1;
			DE_CORE_ASSERT(!m_Params.Capabilities.AssertOnOverflow,
				"Renderer3D: more skinned instances in a frame than MaxSkinnedInstances and AssertOnOverflow is set.");

			if (!m_SkinnedBudgetWarned)
			{
				DE_CORE_WARN("Renderer3D: more than {} skinned instances in one frame; the rest are skipped. Raise Renderer3DCapabilities.MaxSkinnedInstances (at most {}).",
					budget, k_MaxSkinnedInstancesLimit);
				m_SkinnedBudgetWarned = true;
			}
		}
	}

	void Renderer3D::UploadSkinData(const SkinnedInstance& instance)
	{
		m_SkinData.Model = instance.Transform;
		m_SkinData.NormalMatrix = glm::mat4(glm::inverseTranspose(glm::mat3(instance.Transform)));
		m_SkinData.Color = instance.Color;
		std::copy_n(m_SkinnedJoints.begin() + instance.FirstJoint, instance.JointCount, m_SkinData.Joints);
		const uint64_t uploadSize = m_FullSkinUploads ? sizeof(SkinData) : offsetof(SkinData, Joints) + instance.JointCount * sizeof(glm::mat4);
		Renderer::Upload(m_SkinBuffer, &m_SkinData, uploadSize);
	}

	void Renderer3D::EnsureSkinBuffers(const Mesh* mesh)
	{
		if (mesh->m_SkinVertexBuffer)
			return;

		const std::vector<SkinnedMeshVertex>& vertices = mesh->GetSkinVertices();
		const std::vector<uint32_t>& indices = mesh->GetIndices();
		mesh->m_SkinVertexBuffer = GraphicsBuffer::CreateVertexBuffer(vertices.size() * sizeof(SkinnedMeshVertex), nullptr, false, "Renderer3D_SkinVB");
		mesh->m_SkinIndexBuffer = GraphicsBuffer::CreateIndexBuffer(indices.size() * sizeof(uint32_t), nullptr, false, "Renderer3D_SkinIB", GraphicsFormat::Uint32);
		Renderer::Upload(mesh->m_SkinVertexBuffer, vertices.data(), vertices.size() * sizeof(SkinnedMeshVertex));
		Renderer::Upload(mesh->m_SkinIndexBuffer, indices.data(), indices.size() * sizeof(uint32_t));
	}

	void Renderer3D::DrawSkinnedSubmissions()
	{
		if (m_SkinnedSubmissions.empty())
			return;

		DE_PROFILE_SCOPE("Renderer3D::DrawSkinnedSubmissions");
		Texture* shadowAtlas = m_ShadowAtlas ? m_ShadowAtlas->GetDepthAttachment() : m_PlaceholderShadowAtlas;
		uint32_t uploadedInstance = ~0u;
		for (const SkinnedSubmission& submission : m_SkinnedSubmissions)
		{
			if (!submission.Translucent)
				DrawSkinnedSubmission(submission, uploadedInstance, shadowAtlas);
		}
	}

	bool Renderer3D::DrawSkinnedSubmission(const SkinnedSubmission& submission, uint32_t& uploadedInstance, Texture* shadowAtlas)
	{
		if (m_SkinnedInstanceDropped[submission.Instance])
		{
			++m_Statistics.DroppedSkinnedDraws;
			return false;
		}
		if (submission.Shadows == ShadowCasting::ShadowsOnly)
			return false;

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
			return false;
		}

		EnsureSkinningResources();
		if (submission.Instance != uploadedInstance)
		{
			uploadedInstance = submission.Instance;
			UploadSkinData(m_SkinnedInstances[submission.Instance]);
		}

		Material* material = ResolveSkinnedMaterial(submission.Material);
		const Mesh* mesh = submission.Mesh;
		EnsureSkinBuffers(mesh);

		material->SetSceneUniformBuffer(m_SceneUniformBuffer);
		material->SetSkinUniformBuffer(m_SkinBuffer);
		material->SetShadowResources(m_ShadowDataBuffer, shadowAtlas, m_ShadowSampler);
		Renderer::DrawIndexed(material, m_SkinnedLayout, mesh->m_SkinVertexBuffer, mesh->m_SkinIndexBuffer, mesh->GetIndexCount());

		++m_Statistics.DrawCalls;
		++m_Statistics.SkinnedDraws;
		++m_Statistics.SubmittedMeshes;
		return true;
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
		m_LastSubmittedLight = {};
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

		// A strength of 0 draws no shadow, so it renders no cascades, as a local light skips its slot.
		const float shadowStrength = glm::clamp(FiniteOr(light.ShadowStrength, 1.0f), 0.0f, 1.0f);
		if (light.CastShadows && shadowStrength > 0.0f)
		{
			if (m_ShadowLight < 0)
			{
				m_ShadowLight = count;
				m_ShadowStrength = shadowStrength;
				m_ShadowCasterGroups = light.ShadowCasterGroups;
			}
			else if (!m_SecondShadowLightWarned)
			{
				DE_CORE_WARN("Renderer3D: a scene has more than one directional light with CastShadows; only the first casts shadows.");
				m_SecondShadowLightWarned = true;
			}
		}

		m_CameraData.DirectionalLights[count] = { glm::vec4(light.Direction, 0.0f), glm::vec4(light.Color * light.Intensity, 0.0f) };
		m_LastSubmittedLight = { count, true };
		++count;
		return true;
	}

	template<typename LightType>
	bool Renderer3D::SubmitLocalLight(const LightType& light)
	{
		m_SceneLightSubmitted = true;
		m_LastSubmittedLight = {};
		if (!Internal::IsUsableLight(light))
			return false;

		LocalLightCandidate* candidate = AddLocalLight();
		if (!candidate)
			return false;
		m_LastSubmittedLight = { static_cast<int32_t>(m_LocalLights.size() - 1), false };

		const Internal::LightCone cone = Internal::GetLightCone(light);
		candidate->Data.PositionRange = glm::vec4(light.Position, light.Range);
		candidate->Data.Color = glm::vec4(light.Color * light.Intensity, cone.Scale);
		candidate->Data.SpotDirection = glm::vec4(cone.Axis, cone.Offset);
		candidate->Brightness = Strength(light.Color) * light.Intensity;
		candidate->CastShadows = light.CastShadows;
		candidate->ShadowCasterGroups = light.ShadowCasterGroups;
		candidate->ShadowStrength = glm::clamp(FiniteOr(light.ShadowStrength, 1.0f), 0.0f, 1.0f);
		if constexpr (std::is_same_v<LightType, SpotLight>)
		{
			// Past 75 degrees one perspective view would need too wide a field of view.
			candidate->OuterConeAngle = glm::clamp(light.OuterConeAngle, 1.0f, 179.0f);
			candidate->ShadowFaces = candidate->OuterConeAngle > 75.0f ? 6u : 1u;
		}
		else
		{
			candidate->ShadowFaces = 6;
		}
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

	bool Renderer3D::SetFog(const Fog& fog)
	{
		if (fog.Mode == FogMode::None)
		{
			ClearFog();
			return true;
		}

		const bool known = fog.Mode == FogMode::Linear || fog.Mode == FogMode::Exponential || fog.Mode == FogMode::ExponentialSquared;
		const bool finite = IsFinite(fog.Color) && std::isfinite(fog.Start) && std::isfinite(fog.End) && std::isfinite(fog.Density) && std::isfinite(fog.MaxOpacity);
		const bool usable = known && finite && fog.Density >= 0.0f && (fog.Mode != FogMode::Linear || fog.End > fog.Start);
		if (!usable)
		{
			if (!m_FogWarned)
			{
				DE_CORE_WARN("Renderer3D: a fog was ignored: it needs finite values, a Density of at least 0 and, for Linear, an End past its Start.");
				m_FogWarned = true;
			}
			return false;
		}

		m_CameraData.FogColor = glm::vec4(fog.Color, std::clamp(fog.MaxOpacity, 0.0f, 1.0f));
		m_CameraData.FogParams = glm::vec4(fog.Start, fog.End, fog.Density, static_cast<float>(fog.Mode));
		return true;
	}

	void Renderer3D::ClearFog()
	{
		m_CameraData.FogColor = glm::vec4(0.0f);
		m_CameraData.FogParams = glm::vec4(0.0f);
	}

	uint32_t Renderer3D::GetLocalLightBudget() const
	{
		return std::min(m_Params.Capabilities.MaxLocalLights, k_MaxLocalLights);
	}

	void Renderer3D::SetLightBudgetFade(float band)
	{
		m_Params.Capabilities.LightBudgetFade = std::clamp(FiniteOr(band, 0.0f), 0.0f, 4.0f);
	}

	void Renderer3D::SetDirectionalLight(const glm::vec3& direction, float ambient)
	{
		m_Params.LightDirection = direction;
		m_Params.Ambient = ambient;
	}

	void Renderer3D::ClearSceneLights()
	{
		m_CameraData.AmbientColor = glm::vec4(0.0f);
		m_CameraData.LightCounts = glm::ivec4(0);
		m_LocalLights.clear();
		m_SceneLightSubmitted = false;
		m_DroppedLights = 0;
		m_ShadowLight = -1;
		m_ShadowCasterGroups = 0xFFFFFFFFu;
		m_LastSubmittedLight = {};
		m_ShadowProbes.clear();
		ClearFog();
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
			candidate.Priority = candidate.Score / (1.0f + candidate.Nearness);
			m_VisibleLocalLights.push_back(index);
		}

		const uint32_t visibleCount = static_cast<uint32_t>(m_VisibleLocalLights.size());
		const uint32_t budget = GetLocalLightBudget();
		const float fadeBand = m_Params.Capabilities.LightBudgetFade;
		uint32_t localCount = visibleCount;
		float fadeCutoff = 0.0f;
		if (visibleCount > budget)
		{
			DE_CORE_ASSERT(!m_Params.Capabilities.AssertOnOverflow,
				"Renderer3D: more point and spot lights in view than MaxLocalLights and AssertOnOverflow is set.");

			if (fadeBand > 0.0f)
			{
				// One past the budget, so the first dropped light's priority is known: a drawn light
				// fades out as it nears it, and is dark by the time the two trade places.
				std::partial_sort(m_VisibleLocalLights.begin(), m_VisibleLocalLights.begin() + budget + 1, m_VisibleLocalLights.end(),
					[this](uint32_t a, uint32_t b)
					{
						const float priorityA = m_LocalLights[a].Priority;
						const float priorityB = m_LocalLights[b].Priority;
						if (priorityA != priorityB)
							return priorityA > priorityB;
						return a < b;
					});
				fadeCutoff = m_LocalLights[m_VisibleLocalLights[budget]].Priority;
			}
			else
			{
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
			}

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
		{
			const LocalLightCandidate& candidate = m_LocalLights[m_VisibleLocalLights[slot]];
			LocalLightData& data = m_CameraData.LocalLights[slot];
			data = candidate.Data;
			if (fadeCutoff > 0.0f)
			{
				const float fade = BudgetFade(candidate.Priority, fadeCutoff, fadeBand);
				data.Color = glm::vec4(glm::vec3(data.Color) * fade, data.Color.w);
				m_Statistics.FadedLights += fade < 1.0f ? 1 : 0;
			}
		}
		m_CameraData.LightCounts.y = static_cast<int>(localCount);
		m_DrawnLocalLights = localCount;

		m_Statistics.DirectionalLights = static_cast<uint32_t>(directionalCount);
		m_Statistics.LocalLights = localCount;
		m_Statistics.CulledLights = static_cast<uint32_t>(m_LocalLights.size()) - visibleCount;
		m_Statistics.DroppedLights = m_DroppedLights;
	}

	void Renderer3D::SubmitMesh(const Mesh* mesh, const glm::mat4& transform, const glm::vec4& color, Material* material, ShadowCasting shadows, uint32_t shadowGroups)
	{
		if (!m_SceneActive || m_SceneSkipped || !mesh)
			return;
		if (shadows == ShadowCasting::Off)
			shadowGroups = 1;

		Material* batchMaterial = material ? material : m_Material;
		MaterialBatch* batch = &m_Batches[BatchKey{ batchMaterial, shadows, shadowGroups }];
		if (!batch->Enqueued)
		{
			// A skinned shader can't draw the static vertex stream, and no SkinData is bound here.
			const Shader* shader = batchMaterial->GetShader();
			batch->Enqueued = true;
			batch->SkinnedOnly = batchMaterial != m_Material && shader && shader->FindUniformBufferBinding(Material::k_SkinDataBlockName) >= 0;
			batch->Translucent = !batch->SkinnedOnly && shadows != ShadowCasting::ShadowsOnly && batchMaterial->IsTranslucent();
			if (!batch->SkinnedOnly && !batch->Translucent)
				m_DrawOrder.push_back(BatchKey{ batchMaterial, shadows, shadowGroups });
			else if (batch->SkinnedOnly && !m_SkinnedOnlyWarned)
			{
				const std::string& name = batchMaterial->GetParams().DebugName;
				DE_CORE_WARN("Renderer3D: material '{}' has a skinned shader, so the static meshes given it draw with the default material.", name.empty() ? "<unnamed>" : name.c_str());
				m_SkinnedOnlyWarned = true;
			}
		}
		if (batch->SkinnedOnly)
		{
			batch = &m_Batches[BatchKey{ m_Material, shadows, shadowGroups }];
			if (!batch->Enqueued)
			{
				batch->Enqueued = true;
				m_DrawOrder.push_back(BatchKey{ m_Material, shadows, shadowGroups });
			}
		}
		MaterialBatch& matBatch = *batch;

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

		// A translucent mesh is kept on its own, indexed from its first vertex, until EndScene sorts it.
		std::vector<Vertex>* destVertices = &m_TranslucentVertices;
		std::vector<uint32_t>* destIndices = &m_TranslucentIndices;
		uint32_t vertexOffset = 0;
		if (matBatch.Translucent)
		{
			TranslucentSubmission& submission = m_TranslucentSubmissions.emplace_back();
			submission.Material = batchMaterial;
			submission.FirstVertex = static_cast<uint32_t>(m_TranslucentVertices.size());
			submission.VertexCount = static_cast<uint32_t>(vertices.size());
			submission.FirstIndex = static_cast<uint32_t>(m_TranslucentIndices.size());
			submission.IndexCount = static_cast<uint32_t>(indices.size());
			submission.Shadows = shadows;
			submission.ShadowGroups = shadowGroups;
		}
		else
		{
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
			destVertices = &chunk->Vertices;
			destIndices = &chunk->Indices;
			vertexOffset = static_cast<uint32_t>(chunk->Vertices.size());
		}

		// Normals need the inverse-transpose so non-uniform scale (stretched walls)
		// doesn't skew them; positions just use the model matrix.
		const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(transform));

		// A caster's bounds fold into the transform loop rather than a second walk over its vertices.
		const bool casts = shadows != ShadowCasting::Off && !vertices.empty();
		glm::vec3 low = m_HasCasters ? m_CasterMin : glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 high = m_HasCasters ? m_CasterMax : glm::vec3(std::numeric_limits<float>::lowest());
		for (const MeshVertex& v : vertices)
		{
			Vertex vertex;
			vertex.Position = glm::vec3(transform * glm::vec4(v.Position, 1.0f));
			vertex.Normal = normalMatrix * v.Normal;
			vertex.Color = color;
			vertex.TexCoord = v.TexCoord;
			destVertices->push_back(vertex);
			if (casts)
			{
				low = glm::min(low, vertex.Position);
				high = glm::max(high, vertex.Position);
			}
		}

		if (casts)
		{
			++m_StaticCasters;
			m_CasterMin = low;
			m_CasterMax = high;
			m_HasCasters = true;
		}

		for (uint32_t index : indices)
			destIndices->push_back(index + vertexOffset);

		if (matBatch.Translucent)
		{
			TranslucentSubmission& submission = m_TranslucentSubmissions.back();
			glm::vec3 center = glm::vec3(transform[3]);
			if (submission.VertexCount > 0)
			{
				glm::vec3 sum(0.0f);
				for (uint32_t i = 0; i < submission.VertexCount; ++i)
					sum += m_TranslucentVertices[submission.FirstVertex + i].Position;
				center = sum / static_cast<float>(submission.VertexCount);
			}
			submission.SortKey = TranslucentSortKey(center);
		}

		++m_Statistics.SubmittedMeshes;
		m_Statistics.VertexCount += static_cast<uint32_t>(vertices.size());
		m_Statistics.IndexCount += static_cast<uint32_t>(indices.size());
	}

	void Renderer3D::SetShadowSettings(const Renderer3DShadowSettings& settings)
	{
		Renderer3DShadowSettings clamped = settings;
		clamped.AtlasSize = FloorPowerOfTwo(std::clamp(settings.AtlasSize, 2048u, 8192u));
		clamped.CascadeCount = std::clamp(settings.CascadeCount, 1u, k_MaxShadowCascades);
		clamped.CascadeResolution = FloorPowerOfTwo(std::clamp(settings.CascadeResolution, 64u, clamped.AtlasSize / 2));
		clamped.MaxDistance = std::max(FiniteOr(settings.MaxDistance, 60.0f), 0.01f);
		clamped.SplitLambda = std::clamp(FiniteOr(settings.SplitLambda, 0.75f), 0.0f, 1.0f);
		clamped.CascadeBlend = std::clamp(FiniteOr(settings.CascadeBlend, 0.1f), 0.0f, 0.5f);
		clamped.SlopeBias = std::max(FiniteOr(settings.SlopeBias, 2.0f), 0.0f);
		clamped.NormalBias = std::max(FiniteOr(settings.NormalBias, 1.5f), 0.0f);
		clamped.LocalShadowResolution = FloorPowerOfTwo(std::clamp(settings.LocalShadowResolution, 128u, clamped.AtlasSize / 2));

		// The biases are baked into the shadow pipelines.
		const bool biasChanged = clamped.DepthBias != m_Params.Shadows.DepthBias || clamped.SlopeBias != m_Params.Shadows.SlopeBias;
		m_Params.Shadows = clamped;
		if (biasChanged)
			DestroyShadowMaterials();
		if (m_ShadowAtlas && m_ShadowAtlas->GetWidth() != clamped.AtlasSize)
			m_ShadowAtlas->Resize(clamped.AtlasSize, clamped.AtlasSize);
	}

	void Renderer3D::EnsureShadowResources()
	{
		if (!m_ShadowAtlas)
		{
			m_ShadowAtlas = Framebuffer::Create(FramebufferParams()
				.SetDebugName("Renderer3D_ShadowAtlas")
				.SetWidth(static_cast<int32_t>(m_Params.Shadows.AtlasSize))
				.SetHeight(static_cast<int32_t>(m_Params.Shadows.AtlasSize))
				.SetDepthSampleable(true));

			m_ShadowViewsBuffer = GraphicsBuffer::Create(GraphicsBufferParams()
				.SetDebugName("Renderer3D_ShadowViews")
				.SetByteSize(sizeof(ShadowViews))
				.SetType(BufferType::UniformBuffer)
				.SetIsVolatile(true)
				.SetDirectUpload(false)
				.SetMaxWritesPerFrame(k_MaxScenesPerFrame));

			m_ShadowShader = Internal::CreateEngineShader(ShaderParams().SetName("Renderer3DShadow"), "Renderer3D_Shadow.glsl");
			Internal::WatchUnmanagedShader(m_ShadowShader);
		}
	}

	Material* Renderer3D::GetShadowMaterial(bool skinned, uint32_t shadowGroups)
	{
		std::unordered_map<uint32_t, Material*>& materials = skinned ? m_SkinnedShadowMaterials : m_ShadowMaterials;
		Material*& material = materials[shadowGroups];
		if (!material)
		{
			material = Material::Create(MaterialParams()
				.SetDebugName(skinned ? "Renderer3D_SkinnedShadow" : "Renderer3D_Shadow")
				.SetShader(skinned ? m_SkinnedShadowShader : m_ShadowShader)
				.SetCullMode(CullMode::None)
				.SetBlendMode(BlendMode::Opaque)
				.SetDepthBias(m_Params.Shadows.DepthBias, m_Params.Shadows.SlopeBias));
			material->SetUniform(glm::uvec4(shadowGroups, 0u, 0u, 0u));
		}
		material->SetSceneUniformBuffer(m_ShadowViewsBuffer);
		if (skinned)
			material->SetSkinUniformBuffer(m_SkinBuffer);
		return material;
	}

	void Renderer3D::DestroyShadowMaterials()
	{
		for (auto& [groups, material] : m_ShadowMaterials)
			DestroyAndDelete(material);
		for (auto& [groups, material] : m_SkinnedShadowMaterials)
			DestroyAndDelete(material);
		m_ShadowMaterials.clear();
		m_SkinnedShadowMaterials.clear();
	}

	bool Renderer3D::PrepareShadows()
	{
		m_ShadowData = ShadowData();
		m_ShadowViewCount = 0;
		if (!m_HasCasters)
			return false;
		if (!GraphicsContext::Get().SupportsClipDistance())
		{
			if (!m_NoClipDistanceWarned)
			{
				DE_CORE_WARN("Renderer3D: this GPU has no shaderClipDistance, which the shadow pass needs to keep each view inside its atlas tile; shadows are off.");
				m_NoClipDistanceWarned = true;
			}
			return false;
		}

		const Renderer3DShadowSettings& settings = m_Params.Shadows;
		ShadowAtlasAllocator allocator(settings.AtlasSize);
		const bool cascades = m_ShadowLight >= 0 && PrepareCascades(allocator);
		PrepareLocalShadows(allocator);
		if (m_ShadowViewCount == 0)
			return false;

		m_ShadowData.ShadowCounts.z = cascades && settings.DebugCascades ? 1 : 0;
		m_ShadowData.ShadowParams.y = settings.NormalBias;
		m_ShadowData.ShadowParams.z = 1.0f / static_cast<float>(settings.AtlasSize);
		m_Statistics.ShadowViews = m_ShadowViewCount;
		EnsureShadowResources();
		return true;
	}

	void Renderer3D::AddShadowTile(const glm::mat4& viewProjection, const glm::uvec2& corner, uint32_t size, const glm::vec4& params, uint32_t casterGroups)
	{
		const float atlasSize = static_cast<float>(m_Params.Shadows.AtlasSize);
		const float half = static_cast<float>(size) * 0.5f;
		const float tileScale = static_cast<float>(size) / atlasSize;
		const glm::vec2 tileCenter(
			(static_cast<float>(corner.x) + half) / atlasSize * 2.0f - 1.0f,
			1.0f - (static_cast<float>(corner.y) + half) / atlasSize * 2.0f);

		m_ShadowViews.Views[m_ShadowViewCount] = { viewProjection, glm::vec4(tileCenter, tileScale, tileScale), glm::uvec4(casterGroups, 0u, 0u, 0u) };
		m_ShadowData.Tiles[m_ShadowViewCount] = { viewProjection,
			glm::vec4(static_cast<float>(corner.x) / atlasSize, static_cast<float>(corner.y) / atlasSize, tileScale, tileScale), params };
		++m_ShadowViewCount;
	}

	bool Renderer3D::PrepareCascades(ShadowAtlasAllocator& allocator)
	{
		const Renderer3DShadowSettings& settings = m_Params.Shadows;
		const glm::mat4 inverse = glm::inverse(m_CameraData.ViewProjection);
		auto unproject = [&inverse](float x, float y, float z)
		{
			const glm::vec4 point = inverse * glm::vec4(x, y, z, 1.0f);
			return glm::vec3(point) / point.w;
		};

		// The view's frustum corner rays, near (z = 0) to far (z = 1).
		const glm::vec2 cornersXY[4] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
		glm::vec3 nearCorners[4], farCorners[4];
		for (int i = 0; i < 4; ++i)
		{
			nearCorners[i] = unproject(cornersXY[i].x, cornersXY[i].y, 0.0f);
			farCorners[i] = unproject(cornersXY[i].x, cornersXY[i].y, 1.0f);
		}
		const glm::vec3 nearCenter = unproject(0.0f, 0.0f, 0.0f);
		const glm::vec3 farCenter = unproject(0.0f, 0.0f, 1.0f);
		if (!IsFinite(nearCenter) || !IsFinite(farCenter) || glm::length(farCenter - nearCenter) < 1e-6f)
			return false;

		const bool perspective = m_CameraData.CameraPosition.w > 0.5f;
		const glm::vec3 forward = glm::normalize(farCenter - nearCenter);
		const glm::vec3 origin = perspective ? glm::vec3(m_CameraData.CameraPosition) : nearCenter;
		const float nearDepth = std::max(glm::dot(nearCenter - origin, forward), 0.0f);
		const float farDepth = std::min(glm::dot(farCenter - origin, forward), settings.MaxDistance);
		if (farDepth <= nearDepth)
			return false;

		const glm::vec3 lightDirection = glm::normalize(glm::vec3(m_CameraData.DirectionalLights[m_ShadowLight].Direction));
		if (!IsFinite(lightDirection))
			return false;

		const uint32_t cascadeCount = perspective ? settings.CascadeCount : 1u;
		const uint32_t resolution = settings.CascadeResolution;

		// A point along a corner ray at a depth along the view: depth is linear along every ray.
		auto atDepth = [&](int corner, float depth)
		{
			const float nearAlong = glm::dot(nearCorners[corner] - origin, forward);
			const float farAlong = glm::dot(farCorners[corner] - origin, forward);
			const float t = farAlong - nearAlong > 1e-6f ? (depth - nearAlong) / (farAlong - nearAlong) : 0.0f;
			return nearCorners[corner] + (farCorners[corner] - nearCorners[corner]) * t;
		};

		const glm::vec3 up = std::abs(lightDirection.y) > 0.99f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		float sliceStart = nearDepth;
		uint32_t made = 0;
		for (uint32_t cascade = 0; cascade < cascadeCount; ++cascade)
		{
			// The practical split scheme: a blend of even and logarithmic splits.
			const float fraction = static_cast<float>(cascade + 1) / static_cast<float>(cascadeCount);
			const float even = nearDepth + (farDepth - nearDepth) * fraction;
			const float logarithmic = nearDepth > 1e-3f ? nearDepth * std::pow(farDepth / nearDepth, fraction) : even;
			const float sliceEnd = cascade + 1 == cascadeCount ? farDepth : glm::mix(even, logarithmic, settings.SplitLambda);

			glm::vec3 corners[8];
			glm::vec3 center(0.0f);
			for (int corner = 0; corner < 4; ++corner)
			{
				corners[corner] = atDepth(corner, sliceStart);
				corners[corner + 4] = atDepth(corner, sliceEnd);
			}
			for (const glm::vec3& corner : corners)
				center += corner / 8.0f;

			// A sphere's size doesn't change as the camera turns, and rounding it up keeps float noise
			// from changing it either: with the texel snap below, a still light's shadows don't shimmer.
			float radius = 0.0f;
			for (const glm::vec3& corner : corners)
				radius = std::max(radius, glm::length(corner - center));
			radius = std::ceil(radius * 16.0f) / 16.0f;

			const glm::mat4 view = glm::lookAt(center, center + lightDirection, up);

			// From the casters nearest the light to the far side of the slice.
			float casterReach = -radius;
			for (int corner = 0; corner < 8; ++corner)
			{
				const glm::vec3 point((corner & 1) ? m_CasterMax.x : m_CasterMin.x, (corner & 2) ? m_CasterMax.y : m_CasterMin.y, (corner & 4) ? m_CasterMax.z : m_CasterMin.z);
				casterReach = std::min(casterReach, glm::dot(point - center, lightDirection));
			}
			const glm::mat4 projection = glm::orthoRH_ZO(-radius, radius, -radius, radius, casterReach - 1.0f, radius + 1.0f);
			glm::mat4 viewProjection = projection * view;

			const float halfResolution = static_cast<float>(resolution) * 0.5f;
			const glm::vec4 projectedOrigin = viewProjection * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			const glm::vec2 snapped = glm::round(glm::vec2(projectedOrigin) * halfResolution) / halfResolution;
			viewProjection[3][0] += snapped.x - projectedOrigin.x;
			viewProjection[3][1] += snapped.y - projectedOrigin.y;

			glm::uvec2 corner;
			if (!allocator.Allocate(resolution, corner))
				break;

			AddShadowTile(viewProjection, corner, resolution, glm::vec4(sliceEnd, 2.0f * radius / static_cast<float>(resolution), 0.0f, 0.0f), m_ShadowCasterGroups);
			m_Statistics.ShadowCascadeEnds[cascade] = sliceEnd;
			++made;
			sliceStart = sliceEnd;
		}

		if (made == 0)
			return false;

		m_ShadowData.ShadowCounts.x = static_cast<int>(made);
		m_ShadowData.ShadowCounts.y = m_ShadowLight;
		m_ShadowData.ShadowOrigin = glm::vec4(origin, 0.0f);
		m_ShadowData.ShadowForward = glm::vec4(forward, settings.CascadeBlend);
		m_ShadowData.ShadowParams.x = m_ShadowStrength;
		m_ShadowData.ShadowParams.w = farDepth;
		m_Statistics.ShadowCascades = made;
		return true;
	}

	void Renderer3D::PrepareLocalShadows(ShadowAtlasAllocator& allocator)
	{
		// Tiers are remembered by submission index, which only names the same light while the scene
		// submits the same lights in the same order, and per scene of the frame, so a main view and a
		// minimap with as many lights don't overwrite each other's every frame.
		const size_t table = std::min<size_t>(m_SceneOfFrame, k_MaxScenesPerFrame - 1);
		if (m_LocalShadowTiers.size() <= table)
			m_LocalShadowTiers.resize(table + 1);
		std::vector<uint8_t>& tiers = m_LocalShadowTiers[table];
		if (tiers.size() != m_LocalLights.size())
			tiers.assign(m_LocalLights.size(), 0xFF);

		const Renderer3DShadowSettings& settings = m_Params.Shadows;
		const uint32_t cap = std::min(m_Params.Capabilities.MaxShadowedLocalLights, k_MaxShadowedLocalLights);
		const float fadeBand = m_Params.Capabilities.LightBudgetFade;

		// The drawn casting lights. One whose range holds no caster can't be shadowed.
		std::array<uint32_t, k_MaxLocalLights> casting{};
		uint32_t castingCount = 0;
		for (uint32_t slot = 0; slot < m_DrawnLocalLights; ++slot)
		{
			const LocalLightCandidate& light = m_LocalLights[m_VisibleLocalLights[slot]];
			if (!light.CastShadows || light.ShadowStrength <= 0.0f)
				continue;
			if (!SphereTouchesBox(glm::vec3(light.Data.PositionRange), light.Data.PositionRange.w, m_CasterMin, m_CasterMax))
				continue;
			casting[castingCount++] = slot;
		}
		if (castingCount == 0)
			return;

		// Ranked as the budget ranks: ResolveSceneLights sorts the drawn slots only when the budget
		// overflows, and the slots themselves must keep their order, which the lit shader sums in.
		std::sort(casting.begin(), casting.begin() + castingCount, [this, fadeBand](uint32_t slotA, uint32_t slotB)
		{
			const uint32_t a = m_VisibleLocalLights[slotA];
			const uint32_t b = m_VisibleLocalLights[slotB];
			const LocalLightCandidate& lightA = m_LocalLights[a];
			const LocalLightCandidate& lightB = m_LocalLights[b];
			if (fadeBand > 0.0f)
			{
				if (lightA.Priority != lightB.Priority)
					return lightA.Priority > lightB.Priority;
				return a < b;
			}
			if (lightA.Score != lightB.Score)
				return lightA.Score > lightB.Score;
			if (lightA.Nearness != lightB.Nearness)
				return lightA.Nearness < lightB.Nearness;
			return a < b;
		});

		if (castingCount > cap)
		{
			m_Statistics.UnshadowedLights += castingCount - cap;
			if (!m_UnshadowedWarned)
			{
				DE_CORE_WARN("Renderer3D: {} point and spot lights with CastShadows are drawn but MaxShadowedLocalLights is {}; the lowest-ranked ones light unshadowed. Raise Renderer3DCapabilities.MaxShadowedLocalLights (at most {}).",
					castingCount, cap, k_MaxShadowedLocalLights);
				m_UnshadowedWarned = true;
			}
		}
		const float shadowCutoff = fadeBand > 0.0f && castingCount > cap ? m_LocalLights[m_VisibleLocalLights[casting[cap]]].Priority : 0.0f;

		auto tierFor = [](uint32_t rank) -> uint8_t { return rank < 2 ? 0 : rank < 6 ? 1 : 2; };

		static const glm::vec3 k_FaceDirections[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		static const glm::vec3 k_FaceUps[6] = { { 0, 1, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 0 } };

		int shadowed = 0;
		for (uint32_t rank = 0; rank < std::min(castingCount, cap); ++rank)
		{
			const uint32_t slot = casting[rank];
			const uint32_t index = m_VisibleLocalLights[slot];
			const LocalLightCandidate& light = m_LocalLights[index];

			uint8_t tier = tierFor(rank);
			const uint8_t previous = tiers[index];
			if (previous != 0xFF && previous != tier && (tierFor(rank > 0 ? rank - 1 : 0) == previous || tierFor(rank + 1) == previous))
				tier = previous;
			tiers[index] = tier;

			const glm::vec3 position(light.Data.PositionRange);
			const float range = light.Data.PositionRange.w;
			const float nearPlane = std::max(0.05f, range * 0.002f);
			const uint32_t baseSize = light.ShadowFaces == 6 ? settings.LocalShadowResolution / 2 : settings.LocalShadowResolution;
			const uint32_t wanted = std::max(baseSize >> tier, 64u);

			const uint32_t firstTile = m_ShadowViewCount;
			bool complete = true;
			for (uint32_t face = 0; face < light.ShadowFaces; ++face)
			{
				uint32_t size = wanted;
				glm::uvec2 corner;
				while (!allocator.Allocate(size, corner))
				{
					size /= 2;
					if (size < 64)
						break;
				}
				if (size < 64)
				{
					complete = false;
					break;
				}

				glm::vec3 direction, up;
				float tangent = 1.0f;
				if (light.ShadowFaces == 6)
				{
					direction = k_FaceDirections[face];
					up = k_FaceUps[face];
				}
				else
				{
					direction = glm::vec3(light.Data.SpotDirection);
					up = std::abs(direction.y) > 0.99f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
					tangent = std::tan(glm::radians(light.OuterConeAngle));
				}
				const float halfTangent = TileHalfAngleTangent(tangent, size);
				const glm::mat4 projection = glm::perspectiveRH_ZO(2.0f * std::atan(halfTangent), 1.0f, nearPlane, range);
				const glm::mat4 view = glm::lookAt(position, position + direction, up);
				AddShadowTile(projection * view, corner, size, glm::vec4(0.0f, 2.0f * halfTangent / static_cast<float>(size), 1.0f, 0.0f), light.ShadowCasterGroups);
			}

			if (!complete)
			{
				m_ShadowViewCount = firstTile;
				++m_Statistics.UnshadowedLights;
				if (!m_AtlasFullWarned)
				{
					DE_CORE_WARN("Renderer3D: the shadow atlas ({} x {}) has no room left for a casting point or spot light, which lights unshadowed. Raise Renderer3DShadowSettings.AtlasSize or lower LocalShadowResolution.",
						settings.AtlasSize, settings.AtlasSize);
					m_AtlasFullWarned = true;
				}
				continue;
			}

			const float fade = shadowCutoff > 0.0f ? BudgetFade(light.Priority, shadowCutoff, fadeBand) : 1.0f;
			m_ShadowData.LocalShadows[slot].Record = glm::vec4(static_cast<float>(firstTile), static_cast<float>(light.ShadowFaces), light.ShadowStrength * fade, 0.0f);
			m_ShadowData.LocalShadows[slot].Position = glm::vec4(position, 0.0f);
			++shadowed;
		}

		m_ShadowData.ShadowCounts.w = shadowed;
		m_Statistics.ShadowedLights = static_cast<uint32_t>(shadowed);
	}

	void Renderer3D::DrawShadowPass()
	{
		DE_PROFILE_SCOPE("Renderer3D::DrawShadowPass");
		Renderer::BeginGpuTimer("Shadows");

		Internal::RenderTargetScope scope(m_ShadowAtlas);
		Renderer::Clear(m_ShadowAtlas, glm::vec4(0.0f));
		Renderer::Upload(m_ShadowViewsBuffer, &m_ShadowViews, sizeof(ShadowViews));

		for (const ChunkDraw& draw : m_ChunkDraws)
		{
			if (draw.Key.Shadows == ShadowCasting::Off)
				continue;

			Renderer::DrawIndexed(GetShadowMaterial(false, draw.Key.ShadowGroups), m_Layout, m_BatchVertexBuffers[draw.Buffer], m_BatchIndexBuffers[draw.Buffer], draw.IndexCount, m_ShadowViewCount);
			++m_Statistics.ShadowDrawCalls;
		}
		m_Statistics.ShadowCasters += m_StaticCasters;

		// The lit pass's skip rule, so a mesh it doesn't draw leaves no shadow without a body.
		auto skinnedShadowDrawn = [this](const SkinnedSubmission& submission)
		{
			const Material* material = submission.Material ? submission.Material : m_Material;
			return submission.Shadows != ShadowCasting::Off && !m_SkinnedInstanceDropped[submission.Instance] &&
				!(IsLitShader(material->GetShader()) && BindsPastSlotZero(*material));
		};
		bool skinnedCasts = false;
		for (const SkinnedSubmission& submission : m_SkinnedSubmissions)
			skinnedCasts = skinnedCasts || skinnedShadowDrawn(submission);

		if (skinnedCasts)
		{
			EnsureSkinningResources();
			if (!m_SkinnedShadowShader)
			{
				m_SkinnedShadowShader = Internal::CreateEngineShader(ShaderParams().SetName("Renderer3DSkinnedShadow").AddDefine("DE_SKINNED"), "Renderer3D_Shadow.glsl");
				Internal::WatchUnmanagedShader(m_SkinnedShadowShader);
			}
			uint32_t uploadedInstance = ~0u;
			for (const SkinnedSubmission& submission : m_SkinnedSubmissions)
			{
				if (!skinnedShadowDrawn(submission))
					continue;

				if (submission.Instance != uploadedInstance)
				{
					uploadedInstance = submission.Instance;
					UploadSkinData(m_SkinnedInstances[submission.Instance]);
				}

				EnsureSkinBuffers(submission.Mesh);
				Renderer::DrawIndexed(GetShadowMaterial(true, submission.ShadowGroups), m_SkinnedShadowLayout, submission.Mesh->m_SkinVertexBuffer, submission.Mesh->m_SkinIndexBuffer, submission.Mesh->GetIndexCount(), m_ShadowViewCount);
				++m_Statistics.ShadowDrawCalls;
				++m_Statistics.ShadowCasters;
			}
		}

		Renderer::EndGpuTimer();
	}

	bool Renderer3D::AddShadowProbe(ShadowProbeLight light, const glm::vec3& point, uint64_t key, float clearance)
	{
		if (!light.IsValid() || !IsFinite(point) || !std::isfinite(clearance))
			return false;
		if (m_ShadowProbes.size() >= k_MaxShadowProbes)
		{
			if (!m_ProbeOverflowWarned)
			{
				DE_CORE_WARN("Renderer3D: more than {} shadow probes in one scene; the rest are ignored.", k_MaxShadowProbes);
				m_ProbeOverflowWarned = true;
			}
			return false;
		}
		m_ShadowProbes.push_back({ light, point, key, (std::max)(clearance, 0.0f) });
		return true;
	}

	std::optional<float> Renderer3D::GetShadowProbeResult(uint64_t key) const
	{
		const auto it = m_ProbeAnswers->ByKey.find(key);
		if (it == m_ProbeAnswers->ByKey.end())
			return std::nullopt;
		return it->second.Value;
	}

	void Renderer3D::ResolveShadowProbes()
	{
		m_GpuProbeKeys.clear();
		if (m_ShadowProbes.empty())
			return;

		const uint64_t frame = Renderer::GetFrameIndex();
		std::unordered_map<uint64_t, ShadowProbeAnswer>& answers = m_ProbeAnswers->ByKey;
		if (frame - m_ProbePruneFrame > 256)
		{
			m_ProbePruneFrame = frame;
			std::erase_if(answers, [frame](const auto& entry) { return entry.second.Frame + 600 < frame; });
		}

		std::vector<int32_t> slotOf(m_LocalLights.size(), -1);
		for (uint32_t slot = 0; slot < m_DrawnLocalLights; ++slot)
			slotOf[m_VisibleLocalLights[slot]] = static_cast<int32_t>(slot);

		for (const ShadowProbe& probe : m_ShadowProbes)
		{
			float light = -1.0f;
			bool shadowed = false;
			if (probe.Light.Directional)
			{
				shadowed = m_ShadowData.ShadowCounts.x > 0 && probe.Light.Index == m_ShadowData.ShadowCounts.y;
			}
			else if (static_cast<size_t>(probe.Light.Index) < slotOf.size())
			{
				const int32_t slot = slotOf[probe.Light.Index];
				shadowed = slot >= 0 && m_ShadowData.LocalShadows[slot].Record.x >= 0.0f;
				light = static_cast<float>(slot);
			}

			// What is drawn: no shadow, so the whole light.
			if (!shadowed)
			{
				ShadowProbeAnswer& answer = answers[probe.Key];
				answer = { 1.0f, frame };
				continue;
			}

			glm::vec3 point = probe.Point;
			if (probe.Clearance > 0.0f)
			{
				if (probe.Light.Directional)
				{
					const glm::vec3 travel(m_CameraData.DirectionalLights[probe.Light.Index].Direction);
					if (glm::dot(travel, travel) > 0.0f)
						point -= glm::normalize(travel) * probe.Clearance;
				}
				else
				{
					const glm::vec3 toLight = glm::vec3(m_LocalLights[probe.Light.Index].Data.PositionRange) - point;
					const float distance = glm::length(toLight);
					if (distance > 0.0f)
						point += toLight * ((std::min)(probe.Clearance, distance) / distance);
				}
			}

			m_ShadowProbeData.Probes[m_GpuProbeKeys.size()] = glm::vec4(point, light);
			m_GpuProbeKeys.push_back(probe.Key);
		}
		// The probe pass shades all k_MaxShadowProbes pixels and indexes the shadow buffers by each
		// probe's w, so an unused entry must hold a valid light: on Vulkan an index past the arrays
		// loses the device.
		std::fill(std::begin(m_ShadowProbeData.Probes) + m_GpuProbeKeys.size(), std::end(m_ShadowProbeData.Probes), glm::vec4(0.0f, 0.0f, 0.0f, -1.0f));
		m_Statistics.ShadowProbes = static_cast<uint32_t>(m_GpuProbeKeys.size());
	}

	void Renderer3D::DrawShadowProbes()
	{
		if (m_GpuProbeKeys.empty())
			return;

		DE_PROFILE_SCOPE("Renderer3D::DrawShadowProbes");
		auto readback = std::ranges::find_if(m_ProbeReadbacks, [](const std::shared_ptr<Internal::TextureReadback>& candidate) { return !candidate->IsBusy(); });
		if (readback == m_ProbeReadbacks.end())
		{
			if (m_ProbeReadbacks.size() >= k_MaxProbeReadbacks)
				return;
			m_ProbeReadbacks.push_back(Internal::TextureReadback::Create());
			readback = m_ProbeReadbacks.end() - 1;
		}

		if (!m_ProbeShader)
		{
			m_ProbeShader = new Internal::FullscreenShader("Renderer3DShadowProbe", "Renderer3D_ShadowProbe.glsl");
			m_ProbeMaterial = m_ProbeShader->CreateMaterial("Renderer3D_ShadowProbe");
			m_ProbeBuffer = GraphicsBuffer::Create(GraphicsBufferParams()
				.SetDebugName("Renderer3D_ShadowProbes")
				.SetByteSize(sizeof(ShadowProbeData))
				.SetType(BufferType::UniformBuffer)
				.SetIsVolatile(true)
				.SetDirectUpload(false)
				.SetMaxWritesPerFrame(k_MaxScenesPerFrame));
			m_ProbeTarget = Framebuffer::Create(FramebufferParams()
				.SetDebugName("Renderer3D_ShadowProbeTarget")
				.SetWidth(static_cast<int32_t>(k_MaxShadowProbes))
				.SetHeight(1)
				.AddAttachment({ TextureFormat::R8 }));
		}

		Renderer::Upload(m_ProbeBuffer, &m_ShadowProbeData, sizeof(ShadowProbeData));
		m_ProbeMaterial->SetSceneUniformBuffer(m_ProbeBuffer);
		m_ProbeMaterial->SetShadowResources(m_ShadowDataBuffer, m_ShadowAtlas->GetDepthAttachment(), m_ShadowSampler);
		Internal::DrawFullscreen(m_ProbeMaterial, m_ProbeTarget);

		// The copy is recorded here, so the next scene's probes can draw into the same target.
		const std::weak_ptr<ShadowProbeAnswers> answers = m_ProbeAnswers;
		(*readback)->Read(m_ProbeTarget->GetAttachment(0), [answers, keys = m_GpuProbeKeys, frame = Renderer::GetFrameIndex()](const TexturePixels& pixels)
		{
			const std::shared_ptr<ShadowProbeAnswers> target = answers.lock();
			if (!target || pixels.Data.empty())
				return;
			for (uint32_t i = 0; i < keys.size() && i < pixels.Width; ++i)
				target->ByKey[keys[i]] = { pixels.GetPixel(i, 0).r, frame };
		});
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
