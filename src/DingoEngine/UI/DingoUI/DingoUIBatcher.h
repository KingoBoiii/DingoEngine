#pragma once
#include "DingoUIDrawList.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace Dingo
{
	class Framebuffer;
	class GraphicsBuffer;
	class Pipeline;
	class RenderPass;
	class Shader;
}

namespace Dingo::Internal
{

	// Draws a UIDrawList with DingoUI.glsl: one vertex stream, one draw per k_UITextureSlots window.
	// An unchanged list (by UIDrawList::Hash, the target's format and size) draws last frame's
	// vertices again without building or uploading them.
	class DingoUIBatcher
	{
	public:
		DingoUIBatcher();
		~DingoUIBatcher();

		DingoUIBatcher(const DingoUIBatcher&) = delete;
		DingoUIBatcher& operator=(const DingoUIBatcher&) = delete;

		// Into target, which must be the current render target, in a frame that renders.
		void Draw(const UIDrawList& drawList, Framebuffer* target, UIRenderStatistics& statistics);
		// Drops the cached vertices when they draw `texture`, so the next Draw rebuilds and rebakes
		// every batch it uses instead of binding the freed texture.
		void ForgetTexture(const Texture* texture);

	private:
		struct TargetPipeline
		{
			uint64_t FormatKey = 0;
			uint32_t ShaderGeneration = 0;
			Dingo::Pipeline* Pipeline = nullptr;
			// One a batch: a baked binding set is immutable, so two batches never share one.
			std::vector<RenderPass*> RenderPasses;
		};

		struct FrameData
		{
			glm::vec2 InvTargetSize{ 0.0f };
			glm::vec2 Padding{ 0.0f };
		};

		TargetPipeline* GetTargetPipeline(Framebuffer* target, uint64_t formatKey);
		void DestroyTargetPipeline(TargetPipeline& targetPipeline);
		void ReserveQuads(uint32_t quadCount, uint64_t frame);
		void ReleaseRetiredBuffers(uint64_t frame);

		Shader* m_Shader = nullptr;
		GraphicsBuffer* m_UniformBuffer = nullptr;
		GraphicsBuffer* m_VertexBuffer = nullptr;
		GraphicsBuffer* m_IndexBuffer = nullptr;
		uint32_t m_QuadCapacity = 0;
		// Replaced buffers, kept until the frames that drew from them are done.
		std::vector<std::pair<GraphicsBuffer*, uint64_t>> m_RetiredBuffers;
		std::vector<TargetPipeline> m_TargetPipelines;

		UIGeometry m_Geometry;
		uint64_t m_UploadedHash = 0;
		bool m_HasUpload = false;
	};

}
