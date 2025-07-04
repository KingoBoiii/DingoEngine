#pragma once
#include "DingoEngine/Graphics/GraphicsBuffer.h"

#include <nvrhi/nvrhi.h>

namespace DingoEngine
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

		virtual const uint32_t GetIndexCount() const override
		{
			if (m_Params.Type == BufferType::IndexBuffer)
			{
				return static_cast<uint32_t>(m_Params.ByteSize / sizeof(uint16_t));
			}

			return 0;
		}

	protected:
		nvrhi::BufferHandle m_BufferHandle;

		friend class CommandList;
		friend class Pipeline;
	};

}
