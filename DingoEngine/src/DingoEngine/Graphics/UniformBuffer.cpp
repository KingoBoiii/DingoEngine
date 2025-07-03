#include "depch.h"
#include "GraphicsUtils.h"
#include "DingoEngine/Graphics/UniformBuffer.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include <nvrhi/utils.h>

namespace DingoEngine
{

	UniformBuffer* UniformBuffer::Create(uint64_t size)
	{
		return new UniformBuffer(size);
	}

	UniformBuffer::UniformBuffer(uint64_t size)
		: m_Size(size)
	{}

	void UniformBuffer::Initialize()
	{
		//nvrhi::BufferDesc bufferDesc = nvrhi::BufferDesc()
		//	.setByteSize(m_Size)
		//	.setInitialState(nvrhi::ResourceStates::ConstantBuffer)
		//	.setKeepInitialState(true)
		//	.setIsConstantBuffer(true)
		//	.setIsVolatile(true)
		//	.setMaxVersions(1); // number of automatic versions, only necessary on Vulkan

		const nvrhi::BufferDesc bufferDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(m_Size, "UniformBuffer", 1)
			//.setCpuAccess(nvrhi::CpuAccessMode::Write) // No CPU access needed for this buffer
			//.setCanHaveRawViews(true) // for storage buffers
			//.setCanHaveUAVs(true) // for storage buffers
			.setInitialState(nvrhi::ResourceStates::ConstantBuffer)
			.setKeepInitialState(true);

		m_BufferHandle = GraphicsContext::GetDeviceHandle()->createBuffer(bufferDesc);
	}

	void UniformBuffer::Destroy()
	{
		if (m_BufferHandle)
		{
			m_BufferHandle->Release();
		}
	}

	void UniformBuffer::Upload(const void* data) 
	{
		//nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
		//	.setQueueType(nvrhi::CommandQueue::Graphics);

		//nvrhi::CommandListHandle commandList = GraphicsContext::GetDeviceHandle()->createCommandList(commandListParameters);

		//Utils::WriteBuffer(commandList, m_BufferHandle, data, m_Size);
		m_Data = data; // Store the data pointer if needed
	}

}
