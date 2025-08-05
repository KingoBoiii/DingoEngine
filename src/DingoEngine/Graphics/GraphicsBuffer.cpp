#include "depch.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "NVRHI/NvrhiGraphicsBuffer.h"

namespace Dingo
{

	GraphicsBuffer* GraphicsBuffer::CreateVertexBuffer(uint64_t size, const void* data, bool directUpload, const std::string& debugName)
	{
		return Create(GraphicsBufferParams()
			.SetDebugName(debugName)
			.SetByteSize(size)
			.SetType(BufferType::VertexBuffer)
			.SetDirectUpload(directUpload)
			.SetInitialData(data));
	}

	GraphicsBuffer* GraphicsBuffer::CreateIndexBuffer(uint64_t size, const void* data, bool directUpload, const std::string& debugName)
	{
		return Create(GraphicsBufferParams()
			.SetDebugName(debugName)
			.SetByteSize(size)
			.SetType(BufferType::IndexBuffer)
			.SetDirectUpload(directUpload)
			.SetInitialData(data));
	}

	GraphicsBuffer* GraphicsBuffer::CreateUniformBuffer(uint64_t size, const void* data, const std::string& debugName)
	{
		return Create(GraphicsBufferParams()
			.SetDebugName(debugName)
			.SetByteSize(size)
			.SetType(Dingo::BufferType::UniformBuffer)
			.SetIsVolatile(true)
			.SetDirectUpload(false));
	}

	GraphicsBuffer* GraphicsBuffer::Create(const GraphicsBufferParams& params)
	{
		GraphicsBuffer* buffer = new NvrhiGraphicsBuffer(params);
		buffer->Initialize();

		if (params.InitialData)
		{
			buffer->Upload(params.InitialData, params.ByteSize);
		}

		return buffer;
	}

}
