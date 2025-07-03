#include "depch.h"
#include "DingoEngine/Graphics/Buffer.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "GraphicsUtils.h"

namespace DingoEngine
{

	VertexBuffer* VertexBuffer::Create(const void* data, uint64_t size)
	{
		return new VertexBuffer(data, size);
	}

	VertexBuffer::VertexBuffer(const void* data, uint64_t size)
		: m_Data(data), m_Size(size)
	{}

	void VertexBuffer::Initialize()
	{
		nvrhi::BufferDesc bufferDesc = nvrhi::BufferDesc()
			.setDebugName("VertexBuffer")
			.setInitialState(nvrhi::ResourceStates::VertexBuffer)
			.setKeepInitialState(true)
			.setIsVertexBuffer(true)
			.setByteSize(m_Size);

		m_BufferHandle = GraphicsContext::GetDeviceHandle()->createBuffer(bufferDesc);

		{
			nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
				.setQueueType(nvrhi::CommandQueue::Graphics);

			nvrhi::CommandListHandle commandList = GraphicsContext::GetDeviceHandle()->createCommandList(commandListParameters);

			commandList->open();

			Utils::WriteBuffer(commandList, m_BufferHandle, m_Data, m_Size);

			commandList->close();

			GraphicsContext::GetDeviceHandle()->executeCommandList(commandList);
			//GraphicsContext::GetDeviceHandle()->waitForIdle();
			//GraphicsContext::GetDeviceHandle()->runGarbageCollection();
		}
	}

	void VertexBuffer::Destroy()
	{
		if (m_BufferHandle)
		{
			m_BufferHandle->Release();
		}
	}

	IndexBuffer* DingoEngine::IndexBuffer::Create(const uint16_t* indices, uint32_t count)
	{
		return new IndexBuffer(indices, count);
	}

	IndexBuffer::IndexBuffer(const uint16_t* indices, uint32_t count)
		: m_Indices(indices), m_Count(count)
	{}

	void IndexBuffer::Initialize()
	{
		nvrhi::BufferDesc bufferDesc = nvrhi::BufferDesc()
			.setDebugName("VertexBuffer")
			.setInitialState(nvrhi::ResourceStates::IndexBuffer)
			.setKeepInitialState(true)
			.setIsIndexBuffer(true)
			.setByteSize(sizeof(uint16_t) * m_Count);

		m_BufferHandle = GraphicsContext::GetDeviceHandle()->createBuffer(bufferDesc);

		{
			nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
				.setQueueType(nvrhi::CommandQueue::Graphics);

			nvrhi::CommandListHandle commandList = GraphicsContext::GetDeviceHandle()->createCommandList(commandListParameters);

			commandList->open();

			Utils::WriteBuffer(commandList, m_BufferHandle, m_Indices, m_Count * sizeof(uint16_t));

			commandList->close();

			GraphicsContext::GetDeviceHandle()->executeCommandList(commandList);
			//GraphicsContext::GetDeviceHandle()->waitForIdle();
			//GraphicsContext::GetDeviceHandle()->runGarbageCollection();
		}
	}

	void IndexBuffer::Destroy()
	{
		if (m_BufferHandle)
		{
			m_BufferHandle->Release();
		}
	}

}
