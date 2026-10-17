#include "depch.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/GraphicsDevice.h"

#include "NVRHI/NvrhiGraphicsBuffer.h"

#include <atomic>

namespace Dingo
{

	uint64_t GraphicsBuffer::AllocateId()
	{
		static std::atomic<uint64_t> s_NextId{ 1 };
		return s_NextId.fetch_add(1, std::memory_order_relaxed);
	}

	GraphicsBuffer* GraphicsBuffer::CreateVertexBuffer(uint64_t size, const void* data, BufferUpload upload, const std::string& debugName)
	{
		return Create(GraphicsBufferParams()
			.SetDebugName(debugName)
			.SetByteSize(size)
			.SetType(BufferType::VertexBuffer)
			.SetUpload(upload)
			.SetInitialData(data));
	}

	GraphicsBuffer* GraphicsBuffer::CreateIndexBuffer(uint64_t size, IndexFormat format, const void* data, BufferUpload upload, const std::string& debugName)
	{
		return Create(GraphicsBufferParams()
			.SetDebugName(debugName)
			.SetByteSize(size)
			.SetType(BufferType::IndexBuffer)
			.SetIndexFormat(format)
			.SetUpload(upload)
			.SetInitialData(data));
	}

	GraphicsBuffer* GraphicsBuffer::CreateUniformBuffer(uint64_t size, const std::string& debugName)
	{
		return Create(GraphicsBufferParams()
			.SetDebugName(debugName)
			.SetByteSize(size)
			.SetType(Dingo::BufferType::UniformBuffer)
			.SetIsVolatile(true)
			.SetUpload(BufferUpload::Deferred));
	}

	GraphicsBuffer* GraphicsBuffer::CreateStorageBuffer(uint64_t size, const std::string& debugName, const void* initialData)
	{
		return Create(GraphicsBufferParams()
			.SetDebugName(debugName)
			.SetByteSize(size)
			.SetType(Dingo::BufferType::StorageBuffer)
			.SetUpload(BufferUpload::Deferred)
			.SetInitialData(initialData));
	}

	GraphicsBuffer* GraphicsBuffer::Create(const GraphicsBufferParams& params)
	{
		if (!Internal::RequireGraphicsDevice("GraphicsBuffer::Create"))
			return nullptr;

		GraphicsBuffer* buffer = new NvrhiGraphicsBuffer(params);
		buffer->Initialize();

		if (params.InitialData)
		{
			buffer->Upload(params.InitialData, params.ByteSize);
		}

		return buffer;
	}

}
