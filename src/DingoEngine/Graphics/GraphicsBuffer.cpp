#include "depch.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "NVRHI/NvrhiGraphicsBuffer.h"

#include <atomic>

namespace Dingo
{

	uint64_t GraphicsBuffer::AllocateId()
	{
		static std::atomic<uint64_t> s_NextId{ 1 };
		return s_NextId.fetch_add(1, std::memory_order_relaxed);
	}

	GraphicsBuffer* GraphicsBuffer::CreateVertexBuffer(uint64_t size, const void* data, bool directUpload, const std::string& debugName)
	{
		return Create(GraphicsBufferParams()
			.SetDebugName(debugName)
			.SetByteSize(size)
			.SetType(BufferType::VertexBuffer)
			.SetDirectUpload(directUpload)
			.SetInitialData(data));
	}

	GraphicsBuffer* GraphicsBuffer::CreateIndexBuffer(uint64_t size, const void* data, bool directUpload, const std::string& debugName, GraphicsFormat indexFormat)
	{
		return Create(GraphicsBufferParams()
			.SetDebugName(debugName)
			.SetByteSize(size)
			.SetType(BufferType::IndexBuffer)
			.SetFormat(indexFormat)
			.SetDirectUpload(directUpload)
			.SetInitialData(data));
	}

	GraphicsBuffer* GraphicsBuffer::CreateUniformBuffer(uint64_t size, const std::string& debugName)
	{
		return Create(GraphicsBufferParams()
			.SetDebugName(debugName)
			.SetByteSize(size)
			.SetType(Dingo::BufferType::UniformBuffer)
			.SetIsVolatile(true)
			.SetDirectUpload(false));
	}

	GraphicsBuffer* GraphicsBuffer::CreateStorageBuffer(uint64_t size, const std::string& debugName)
	{
		return Create(GraphicsBufferParams()
			.SetDebugName(debugName)
			.SetByteSize(size)
			.SetType(Dingo::BufferType::StorageBuffer)
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
