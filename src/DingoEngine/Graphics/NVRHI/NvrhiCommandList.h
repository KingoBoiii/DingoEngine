#pragma once
#include "DingoEngine/Graphics/CommandList.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiCommandList : public Internal::CommandList
	{
	public:
		NvrhiCommandList(const Internal::CommandListParams& params)
			: Internal::CommandList(params)
		{}
		~NvrhiCommandList() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

		virtual void Begin() override;
		virtual void Close() override;
		virtual void Execute() override;
		virtual void End() override;

		virtual bool IsRecording() const override { return m_HasBegun; }

		virtual void Clear(Framebuffer* framebuffer, uint32_t attachmentIndex, const glm::vec4& clearColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f)) override;

		virtual void UploadBuffer(GraphicsBuffer* buffer, const void* data, uint64_t size, uint64_t offset = 0) override;
		virtual void UploadTexture(Texture* texture, const void* data, uint64_t rowPitch) override;

		virtual void SetFramebuffer(Framebuffer* framebuffer) override;
		virtual void SetViewport(const Viewport& viewport) override;
		virtual bool SetPipeline(Pipeline* pipeline) override;
		virtual bool SetRenderPass(RenderPass* renderPass) override;
		virtual void AddVertexBuffer(GraphicsBuffer* vertexBuffer, uint32_t slot = 0, uint64_t offset = 0) override;
		virtual void SetIndexBuffer(GraphicsBuffer* indexBuffer, uint64_t offset = 0) override;
		virtual void SetPushConstants(const void* data, uint32_t size) override;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) override;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0) override;

		virtual bool Dispatch(ComputePass* pass, uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) override;

	public:
		nvrhi::ICommandList* GetNvrhiHandle() const { return m_CommandListHandle; }

		// The storage items of a binding set: raw buffers and storage textures.
		static bool IsStorageItem(const nvrhi::BindingSetItem& item);
		static bool IsRawBufferItem(const nvrhi::BindingSetItem& item)
		{
			return item.type == nvrhi::ResourceType::RawBuffer_SRV || item.type == nvrhi::ResourceType::RawBuffer_UAV;
		}

		// NVRHI's Vulkan backend hands a set's volatile constant buffers their dynamic offsets in the
		// order the desc lists them, and Vulkan applies them in binding order: out of order, each block
		// reads another's version. Every binding set is sorted by slot before it is made.
		static void SortBySlot(nvrhi::BindingSetDesc& desc);

	private:
		// NVRHI places a binding set's barriers only when the bound sets change, so a pass bound twice in
		// a row, or a storage buffer copied between two uses, would get none. Requiring the states again
		// places them (a UAV barrier where the state already is UAV).
		void RequireStorageStates(const std::vector<nvrhi::BindingSetItem>& items);
		// Pushes the constants after setGraphicsState, which invalidates them, and counts a pass break.
		void BeginDraw();

	private:
		bool m_HasBegun = false; // Track if the command list has begun
		nvrhi::GraphicsState m_GraphicsState;
		const std::vector<nvrhi::BindingSetItem>* m_RenderPassStorageItems = nullptr;
		nvrhi::CommandListHandle m_CommandListHandle;

		std::array<uint8_t, nvrhi::c_MaxPushConstantSize> m_PushConstants{};
		uint32_t m_PushConstantSize = 0;

		nvrhi::IFramebuffer* m_LastDrawFramebuffer = nullptr;
		bool m_UploadedSinceDraw = false;
	};

}
