#include "depch.h"
#include "NvrhiGraphicsBuffer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "NvrhiGraphicsContext.h"

namespace Dingo
{

	namespace Utils
	{

		static nvrhi::ResourceStates GetResourceState(BufferType type)
		{
			switch (type)
			{
				case BufferType::VertexBuffer:
					return nvrhi::ResourceStates::VertexBuffer;
				case BufferType::IndexBuffer:
					return nvrhi::ResourceStates::IndexBuffer;
				case BufferType::UniformBuffer:
					return nvrhi::ResourceStates::ConstantBuffer;
					//case BufferType::StorageBuffer:
					//	return nvrhi::ResourceStates::StorageBuffer;
				default:
					return nvrhi::ResourceStates::Unknown;
			}
		}

	}

	void NvrhiGraphicsBuffer::Initialize()
	{
		nvrhi::ResourceStates initialState = Utils::GetResourceState(m_Params.Type);

		nvrhi::BufferDesc bufferDesc = nvrhi::BufferDesc()
			.setDebugName(m_Params.DebugName)
			.setInitialState(initialState)
			.setKeepInitialState(m_Params.KeepInitialState)
			.setIsVertexBuffer(initialState == nvrhi::ResourceStates::VertexBuffer)
			.setIsIndexBuffer(initialState == nvrhi::ResourceStates::IndexBuffer)
			.setIsConstantBuffer(initialState == nvrhi::ResourceStates::ConstantBuffer)
			.setIsVolatile(m_Params.IsVolatile)
			.setByteSize(m_Params.ByteSize);

		if (GraphicsContext::Get().GetParams().GraphicsAPI == GraphicsAPI::Vulkan && initialState == nvrhi::ResourceStates::ConstantBuffer)
		{
			bufferDesc.setMaxVersions(1); // number of automatic versions, only necessary on Vulkan
		}

		m_BufferHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createBuffer(bufferDesc);
	}

	void NvrhiGraphicsBuffer::Destroy()
	{
		if (m_BufferHandle)
		{
			m_BufferHandle->Release();
		}
	}

	void NvrhiGraphicsBuffer::Upload(const void* data, uint64_t size, uint64_t offset)
	{
		DE_CORE_ASSERT(m_BufferHandle, "Buffer handle is not initialized.");
		DE_CORE_ASSERT(data, "Data pointer is null.");
		DE_CORE_ASSERT(size > 0, "Size must be greater than zero.");
		DE_CORE_ASSERT(offset + size <= m_Params.ByteSize, "Upload exceeds buffer size.");

		if (m_Params.DirectUpload)
		{
			nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
				.setQueueType(nvrhi::CommandQueue::Graphics);

			nvrhi::CommandListHandle commandList = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createCommandList(commandListParameters);

			commandList->open();

			commandList->writeBuffer(m_BufferHandle, data, size, offset);

			commandList->close();

			GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->executeCommandList(commandList);
		}

		m_Data = data;
	}

}
