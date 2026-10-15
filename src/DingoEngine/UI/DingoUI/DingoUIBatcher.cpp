#include "depch.h"
#include "DingoUIBatcher.h"

#include "DingoEngine/Asset/UnmanagedShaderWatch.h"
#include "DingoEngine/Graphics/EngineShaders.h"
#include "DingoEngine/Graphics/Framebuffer.h"
#include "DingoEngine/Graphics/FramebufferFormat.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/Pipeline.h"
#include "DingoEngine/Graphics/RenderPass.h"
#include "DingoEngine/Graphics/Renderer.h"

namespace Dingo::Internal
{

	namespace
	{
		constexpr uint32_t k_FrameBinding = 0;
		constexpr uint32_t k_TextureBinding = 1;
		constexpr uint32_t k_SamplerBinding = 2;
		constexpr uint32_t k_PointSamplerBinding = 3;

		constexpr uint32_t k_MinimumQuads = 256;
		// As Renderer3D's arena: longer than any frame stays in flight.
		constexpr uint64_t k_RetireFrames = 8;

		VertexLayout CreateVertexLayout()
		{
			return VertexLayout()
				.SetStride(sizeof(UIVertex))
				.AddAttribute("a_Position", Format::RG32_FLOAT, offsetof(UIVertex, Position))
				.AddAttribute("a_TexCoord", Format::RG32_FLOAT, offsetof(UIVertex, TexCoord))
				.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(UIVertex, Color))
				.AddAttribute("a_BorderColor", Format::RGBA32_FLOAT, offsetof(UIVertex, BorderColor))
				.AddAttribute("a_Local", Format::RGBA32_FLOAT, offsetof(UIVertex, Local))
				.AddAttribute("a_Radii", Format::RGBA32_FLOAT, offsetof(UIVertex, Radii))
				.AddAttribute("a_Clip", Format::RGBA32_FLOAT, offsetof(UIVertex, Clip))
				.AddAttribute("a_Params", Format::RGBA32_FLOAT, offsetof(UIVertex, Params));
		}
	}

	DingoUIBatcher::DingoUIBatcher()
	{
		m_Shader = CreateEngineShader(ShaderParams().SetName("DingoUI"), "DingoUI.glsl");
		if (m_Shader)
			WatchUnmanagedShader(m_Shader);

		m_UniformBuffer = GraphicsBuffer::Create(GraphicsBufferParams()
			.SetDebugName("DingoUI_FrameUBO")
			.SetByteSize(sizeof(FrameData))
			.SetType(BufferType::UniformBuffer)
			.SetIsVolatile(true)
			.SetUpload(BufferUpload::Deferred));
	}

	DingoUIBatcher::~DingoUIBatcher()
	{
		for (TargetPipeline& targetPipeline : m_TargetPipelines)
			DestroyTargetPipeline(targetPipeline);
		m_TargetPipelines.clear();

		for (auto& [buffer, frame] : m_RetiredBuffers)
			DestroyAndDelete(buffer);
		m_RetiredBuffers.clear();

		DestroyAndDelete(m_VertexBuffer);
		DestroyAndDelete(m_IndexBuffer);
		DestroyAndDelete(m_UniformBuffer);

		if (m_Shader)
		{
			UnwatchUnmanagedShader(m_Shader);
			DestroyAndDelete(m_Shader);
		}
	}

	void DingoUIBatcher::Draw(const UIDrawList& drawList, Framebuffer* target, UIRenderStatistics& statistics)
	{
		DE_PROFILE_FUNCTION();

		if (!m_Shader || !m_UniformBuffer || !target || drawList.IsEmpty())
			return;

		const uint32_t width = target->GetWidth();
		const uint32_t height = target->GetHeight();
		if (width == 0 || height == 0)
			return;

		const uint64_t frame = Renderer::GetFrameIndex();
		ReleaseRetiredBuffers(frame);

		const uint64_t formatKey = GetFramebufferFormatKey(target);
		TargetPipeline* targetPipeline = GetTargetPipeline(target, formatKey);
		if (!targetPipeline)
			return;

		uint64_t hash = 0;
		{
			const UICpuTimer hashTimer;
			hash = drawList.Hash(formatKey ^ (static_cast<uint64_t>(width) << 32 | height));
			statistics.HashMs = hashTimer.ElapsedMs();
		}
		if (m_HasUpload && hash == m_UploadedHash)
		{
			statistics.CacheHits = 1;
		}
		else
		{
			{
				DE_PROFILE_SCOPE("DingoUI::Batch");
				const UICpuTimer timer;
				drawList.Build(m_Geometry);
				statistics.BuildMs = timer.ElapsedMs();
			}
			m_HasUpload = false;
			if (m_Geometry.QuadCount == 0)
			{
				statistics.CulledQuads = m_Geometry.CulledQuads;
				return;
			}

			DE_PROFILE_SCOPE("DingoUI::Upload");
			const UICpuTimer uploadTimer;
			ReserveQuads(m_Geometry.QuadCount, frame);
			const uint64_t bytes = sizeof(UIVertex) * m_Geometry.Vertices.size();
			Renderer::Upload(m_VertexBuffer, m_Geometry.Vertices.data(), bytes);

			std::vector<RenderPass*>& renderPasses = targetPipeline->RenderPasses;
			if (renderPasses.size() < m_Geometry.Batches.size())
				renderPasses.resize(m_Geometry.Batches.size(), nullptr);

			Texture* white = Renderer::GetWhiteTexture();
			for (size_t i = 0; i < m_Geometry.Batches.size(); ++i)
			{
				if (!renderPasses[i])
				{
					renderPasses[i] = RenderPass::Create(RenderPassParams().SetPipeline(targetPipeline->Pipeline));
					renderPasses[i]->SetUniformBuffer(k_FrameBinding, m_UniformBuffer);
					renderPasses[i]->SetSampler(k_SamplerBinding, Renderer::GetClampSampler());
					renderPasses[i]->SetSampler(k_PointSamplerBinding, Renderer::GetPointSampler());
				}

				// The shader declares all k_UITextureSlots, so every slot needs a real texture.
				const UIDrawBatch& batch = m_Geometry.Batches[i];
				for (uint32_t slot = 0; slot < k_UITextureSlots; ++slot)
				{
					Texture* texture = slot < batch.TextureCount ? batch.Textures[slot] : nullptr;
					renderPasses[i]->SetTexture(k_TextureBinding, texture ? texture : white, slot);
				}
				renderPasses[i]->Bake();
			}

			// Only a frame that renders gets here, so the upload this hash stands for is recorded.
			m_UploadedHash = hash;
			m_HasUpload = true;
			statistics.Uploads = 1;
			statistics.UploadBytes = bytes;
			statistics.UploadMs = uploadTimer.ElapsedMs();
		}

		statistics.Vertices = static_cast<uint32_t>(m_Geometry.Vertices.size());
		statistics.Quads = m_Geometry.QuadCount;
		statistics.CulledQuads = m_Geometry.CulledQuads;

		DE_PROFILE_SCOPE("DingoUI::Draw");
		const UICpuTimer drawTimer;
		Renderer::BeginGpuTimer("DingoUI");

		const FrameData frameData{ glm::vec2(1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height)) };
		Renderer::Upload(m_UniformBuffer, &frameData, sizeof(frameData));

		for (size_t i = 0; i < m_Geometry.Batches.size(); ++i)
		{
			const UIDrawBatch& batch = m_Geometry.Batches[i];
			Renderer::DrawIndexed(targetPipeline->RenderPasses[i], m_VertexBuffer, m_IndexBuffer, batch.IndexCount, 1, batch.FirstIndex);
			++statistics.DrawCalls;
		}

		Renderer::EndGpuTimer();
		statistics.DrawMs = drawTimer.ElapsedMs();
	}

	void DingoUIBatcher::ForgetTexture(const Texture* texture)
	{
		if (!m_HasUpload)
			return;
		for (const UIDrawBatch& batch : m_Geometry.Batches)
		{
			for (uint32_t slot = 1; slot < batch.TextureCount; ++slot)
			{
				if (batch.Textures[slot] == texture)
				{
					m_HasUpload = false;
					return;
				}
			}
		}
	}

	DingoUIBatcher::TargetPipeline* DingoUIBatcher::GetTargetPipeline(Framebuffer* target, uint64_t formatKey)
	{
		TargetPipeline* targetPipeline = nullptr;
		for (TargetPipeline& candidate : m_TargetPipelines)
		{
			if (candidate.FormatKey == formatKey)
			{
				targetPipeline = &candidate;
				break;
			}
		}

		if (targetPipeline && targetPipeline->Pipeline && targetPipeline->ShaderGeneration == m_Shader->GetGeneration())
			return targetPipeline;

		// Rebuilt here against the live target after a shader reload: the pipeline's own lazy
		// rebuild would read the framebuffer it was created against, which a render target other
		// than the swap chain's may have outlived.
		if (targetPipeline)
			DestroyTargetPipeline(*targetPipeline);
		else
			targetPipeline = &m_TargetPipelines.emplace_back();

		targetPipeline->FormatKey = formatKey;
		targetPipeline->ShaderGeneration = m_Shader->GetGeneration();
		targetPipeline->Pipeline = Pipeline::Create(PipelineParams()
			.SetDebugName("DingoUIPipeline")
			.SetFramebuffer(target)
			.SetShader(m_Shader)
			.SetVertexLayout(CreateVertexLayout())
			.SetCullMode(CullMode::None)
			.SetDepthTest(false)
			.SetDepthWrite(false)
			.SetBlendMode(BlendMode::Alpha));

		// New render passes hold no textures yet.
		m_HasUpload = false;
		return targetPipeline->Pipeline ? targetPipeline : nullptr;
	}

	void DingoUIBatcher::DestroyTargetPipeline(TargetPipeline& targetPipeline)
	{
		for (RenderPass*& renderPass : targetPipeline.RenderPasses)
			DestroyAndDelete(renderPass);
		targetPipeline.RenderPasses.clear();
		DestroyAndDelete(targetPipeline.Pipeline);
	}

	void DingoUIBatcher::ReserveQuads(uint32_t quadCount, uint64_t frame)
	{
		if (quadCount <= m_QuadCapacity)
			return;

		if (m_VertexBuffer)
			m_RetiredBuffers.emplace_back(m_VertexBuffer, frame);
		if (m_IndexBuffer)
			m_RetiredBuffers.emplace_back(m_IndexBuffer, frame);

		m_QuadCapacity = (std::max)({ m_QuadCapacity * 2, quadCount, k_MinimumQuads });

		std::vector<uint32_t> indexData(static_cast<size_t>(m_QuadCapacity) * 6);
		for (uint32_t quad = 0; quad < m_QuadCapacity; ++quad)
		{
			const uint32_t vertex = quad * 4;
			uint32_t* indices = &indexData[static_cast<size_t>(quad) * 6];
			indices[0] = vertex + 0;
			indices[1] = vertex + 1;
			indices[2] = vertex + 2;
			indices[3] = vertex + 2;
			indices[4] = vertex + 3;
			indices[5] = vertex + 0;
		}

		// BufferUpload::Deferred: written in the frame's command list, ordered before its draws.
		m_VertexBuffer = GraphicsBuffer::CreateVertexBuffer(sizeof(UIVertex) * 4ull * m_QuadCapacity, nullptr, BufferUpload::Deferred, "DingoUI_VertexBuffer");
		m_IndexBuffer = GraphicsBuffer::CreateIndexBuffer(sizeof(uint32_t) * indexData.size(), IndexFormat::Uint32, nullptr, BufferUpload::Deferred, "DingoUI_IndexBuffer");
		Renderer::Upload(m_IndexBuffer, indexData.data(), sizeof(uint32_t) * indexData.size());
	}

	void DingoUIBatcher::ReleaseRetiredBuffers(uint64_t frame)
	{
		std::erase_if(m_RetiredBuffers, [frame](std::pair<GraphicsBuffer*, uint64_t>& retired)
		{
			if (retired.second + k_RetireFrames >= frame)
				return false;
			DestroyAndDelete(retired.first);
			return true;
		});
	}

}
