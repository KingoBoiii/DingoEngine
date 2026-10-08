#include "depch.h"
#include "NvrhiGraphicsBuffer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/Renderer.h"
#include "NvrhiCommandList.h"
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

		if (m_Params.Type == BufferType::StorageBuffer)
		{
			UploadStorage(data, size, offset);
			return;
		}

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
		Write(commandList, m_BufferHandle, m_PadsWrites, data, size, offset);
	}

	void NvrhiGraphicsBuffer::Write(nvrhi::ICommandList* commandList, nvrhi::IBuffer* buffer, bool padsWrites, const void* data, uint64_t size, uint64_t offset)
	{
		if (!padsWrites || (size & 3) == 0)
		{
			commandList->writeBuffer(buffer, data, size, offset);
			return;
		}

		// NVRHI copies the data while recording, so the padded copy needn't outlive the call.
		std::vector<uint8_t> padded((size + 3) & ~uint64_t(3), 0);
		std::memcpy(padded.data(), data, size);
		commandList->writeBuffer(buffer, padded.data(), size, offset);
	}

	// ReadBack's timing: in order with the frame's work inside a frame, at once while the render thread is
	// parked, else at the next frame's start. A list of its own mid-frame would run before the frame's
	// earlier dispatches (and on D3D11 clear its state).
	void NvrhiGraphicsBuffer::UploadStorage(const void* data, uint64_t size, uint64_t offset)
	{
		if (CommandList* frameList = Renderer::TryGetRecordingCommandList())
		{
			Write(static_cast<NvrhiCommandList*>(frameList)->GetNvrhiHandle(), data, size, offset);
			return;
		}

		auto writeNow = [](nvrhi::IBuffer* buffer, bool padsWrites, const void* bytes, uint64_t byteCount, uint64_t at)
		{
			nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
			nvrhi::CommandListHandle commandList = device->createCommandList(nvrhi::CommandListParameters().setQueueType(nvrhi::CommandQueue::Graphics));
			commandList->open();
			Write(commandList, buffer, padsWrites, bytes, byteCount, at);
			commandList->close();
			device->executeCommandList(commandList);
		};

		if (Renderer::IsRenderThreadParked())
		{
			writeNow(m_BufferHandle, m_PadsWrites, data, size, offset);
			return;
		}

		std::vector<uint8_t> copy(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
		Renderer::RunAfterFrame([buffer = m_BufferHandle, padsWrites = m_PadsWrites, copy = std::move(copy), offset, writeNow]()
		{
			writeNow(buffer, padsWrites, copy.data(), copy.size(), offset);
		});
	}

	void NvrhiGraphicsBuffer::ReadBack(std::function<void(const std::vector<uint8_t>&)> done, uint64_t offset, uint64_t size)
	{
		if (size == 0 && offset < m_Params.ByteSize)
			size = m_Params.ByteSize - offset;
		if (!m_BufferHandle || m_Params.IsVolatile || size == 0 || offset + size > m_Params.ByteSize)
		{
			DE_CORE_ERROR("GraphicsBuffer::ReadBack: '{}' can't be read there (volatile, or past its end).", m_Params.DebugName);
			done({});
			return;
		}

		nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
		nvrhi::BufferHandle staging = device->createBuffer(nvrhi::BufferDesc()
			.setDebugName(m_Params.DebugName + " (readback)")
			.setByteSize(size)
			.setCpuAccess(nvrhi::CpuAccessMode::Read)
			.setInitialState(nvrhi::ResourceStates::CopyDest)
			.setKeepInitialState(true));

		// Mapping waits for the GPU to finish the copy, on every backend.
		auto resolve = [staging, size, done = std::move(done)]()
		{
			nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
			std::vector<uint8_t> bytes;
			if (const void* mapped = device->mapBuffer(staging, nvrhi::CpuAccessMode::Read))
			{
				bytes.assign(static_cast<const uint8_t*>(mapped), static_cast<const uint8_t*>(mapped) + size);
				device->unmapBuffer(staging);
			}
			done(bytes);
		};

		if (CommandList* frameList = Renderer::TryGetRecordingCommandList())
		{
			// As Texture::ReadPixels: a later use with the binding set already bound wouldn't move the
			// buffer out of the copy source state, so put back the state it had.
			nvrhi::ICommandList* list = static_cast<NvrhiCommandList*>(frameList)->GetNvrhiHandle();
			const nvrhi::ResourceStates state = list->getBufferState(m_BufferHandle);
			list->copyBuffer(staging, 0, m_BufferHandle, offset, size);
			if (state != nvrhi::ResourceStates::Unknown)
			{
				list->setBufferState(m_BufferHandle, state);
				list->commitBarriers();
			}
			Renderer::RunAfterFrame(std::move(resolve));
			return;
		}

		auto copyAndResolve = [source = m_BufferHandle, staging, offset, size, resolve = std::move(resolve)]()
		{
			nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
			nvrhi::CommandListHandle commandList = device->createCommandList(nvrhi::CommandListParameters().setQueueType(nvrhi::CommandQueue::Graphics));
			commandList->open();
			commandList->copyBuffer(staging, 0, source, offset, size);
			commandList->close();
			device->executeCommandList(commandList);
			resolve();
		};

		if (Renderer::IsRenderThreadParked())
			copyAndResolve();
		else
			Renderer::RunAfterFrame(std::move(copyAndResolve));
	}

}
