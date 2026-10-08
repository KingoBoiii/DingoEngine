#include "depch.h"
#include "NvrhiGraphicsBuffer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "NvrhiGraphicsContext.h"

#include <cstring>
#include <vector>

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
				case BufferType::StorageBuffer:
					return nvrhi::ResourceStates::ShaderResource;
				default:
					return nvrhi::ResourceStates::Unknown;
			}
		}

		// NVRHI's Vulkan backend gives each write of a volatile buffer its own version, recycled only once
		// it has seen the GPU finish with it; with none free the write is dropped. Up to four frames can look
		// unfinished: the swap chain's three fence slots, plus NVRHI only polling completion in garbage collection.
		constexpr uint32_t k_VolatileFramesPending = 4;

	}

	void NvrhiGraphicsBuffer::Initialize()
	{
		nvrhi::ResourceStates initialState = Utils::GetResourceState(m_Params.Type);

		// NVRHI's Vulkan backend records a small write to a non-volatile buffer with vkCmdUpdateBuffer,
		// rounded up to a multiple of 4 bytes read from the source and written to the buffer, so the
		// buffer is made long enough to take it and Write pads the source.
		m_PadsWrites = !m_Params.IsVolatile && GraphicsContext::Get().GetGraphicsAPI() == GraphicsAPI::Vulkan;

		nvrhi::BufferDesc bufferDesc = nvrhi::BufferDesc()
			.setDebugName(m_Params.DebugName)
			.setInitialState(initialState)
			.setKeepInitialState(m_Params.KeepInitialState)
			.setIsVertexBuffer(initialState == nvrhi::ResourceStates::VertexBuffer)
			.setIsIndexBuffer(initialState == nvrhi::ResourceStates::IndexBuffer)
			.setIsConstantBuffer(initialState == nvrhi::ResourceStates::ConstantBuffer)
			.setIsVolatile(m_Params.IsVolatile)
			.setByteSize(m_PadsWrites ? (m_Params.ByteSize + 3) & ~uint64_t(3) : m_Params.ByteSize);

		// Read as a ByteAddressBuffer (SPIRV-Cross's SSBO), so D3D11 needs raw views; writable from compute.
		if (m_Params.Type == BufferType::StorageBuffer)
		{
			DE_CORE_ASSERT(!m_Params.IsVolatile, "A storage buffer can't be volatile.");
			bufferDesc.setCanHaveUAVs(true).setCanHaveRawViews(true);
		}

		if (bufferDesc.isVolatile)
		{
			DE_CORE_ASSERT(m_Params.MaxWritesPerFrame > 0, "A volatile buffer needs MaxWritesPerFrame > 0.");
			bufferDesc.setMaxVersions(m_Params.MaxWritesPerFrame * Utils::k_VolatileFramesPending);		}

		m_BufferHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createBuffer(bufferDesc);
	}

	void NvrhiGraphicsBuffer::Destroy()
	{
		m_BufferHandle = nullptr;
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

			Write(commandList, data, size, offset);

			commandList->close();

			GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->executeCommandList(commandList);
		}

		m_Data = data;
	}

	void NvrhiGraphicsBuffer::Write(nvrhi::ICommandList* commandList, const void* data, uint64_t size, uint64_t offset)
	{
		if (!m_PadsWrites || (size & 3) == 0)
		{
			commandList->writeBuffer(m_BufferHandle, data, size, offset);
			return;
		}

		// NVRHI copies the data while recording, so the padded copy needn't outlive the call.
		std::vector<uint8_t> padded((size + 3) & ~uint64_t(3), 0);
		std::memcpy(padded.data(), data, size);
		commandList->writeBuffer(m_BufferHandle, padded.data(), size, offset);
	}

}
