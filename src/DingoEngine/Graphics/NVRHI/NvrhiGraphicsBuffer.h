#pragma once
#include "DingoEngine/Graphics/GraphicsBuffer.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiGraphicsBuffer : public GraphicsBuffer
	{
	public:
		NvrhiGraphicsBuffer(const GraphicsBufferParams& params)
			: GraphicsBuffer(params)
		{}
		virtual ~NvrhiGraphicsBuffer() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;
		virtual void Upload(const void* data, uint64_t size, uint64_t offset = 0ul) override;
		virtual void ReadBack(std::function<void(const std::vector<uint8_t>&)> done, uint64_t offset = 0, uint64_t size = 0) override;
		// Records the write into commandList; every upload of the buffer goes through it.
		void Write(nvrhi::ICommandList* commandList, const void* data, uint64_t size, uint64_t offset);

		virtual const uint32_t GetIndexCount() const override
		{
			if (m_Params.Type == BufferType::IndexBuffer)
			{
				return static_cast<uint32_t>(m_Params.ByteSize / sizeof(uint16_t));
			}

			return 0;
		}

		virtual const void* GetResourceHandle() const override { return m_BufferHandle; }

	protected:
		nvrhi::BufferHandle m_BufferHandle;
		bool m_PadsWrites = false;

		friend class NvrhiCommandList;
		friend class NvrhiPipeline;
		friend class NvrhiRenderPass;
		friend class NvrhiComputePass;
	};

}
