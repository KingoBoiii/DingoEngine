#include "depch.h"
#include "DingoEngine/Graphics/CommandList.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "DingoEngine/Core/Application.h"

#include <nvrhi/utils.h> // for ClearColorAttachment

#include "NVRHI/NvrhiGraphicsBuffer.h"
#include "NVRHI/NvrhiPipeline.h"

namespace DingoEngine
{

	CommandList* CommandList::Create(const CommandListParams& params)
	{
		return new CommandList(params);
	}

	CommandList::CommandList(const CommandListParams& params)
		: m_Params(params)
	{}

	void CommandList::Initialize()
	{
		DE_CORE_ASSERT(m_Params.TargetSwapChain, "CommandList must target a swap chain - does not support otherwise atm.");

		nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
			//.setEnableImmediateExecution(false) // Set to true for immediate execution, false for deferred execution
			.setQueueType(nvrhi::CommandQueue::Graphics);

		m_CommandListHandle = GraphicsContext::GetDeviceHandle()->createCommandList(commandListParameters);
	}

	void CommandList::Destroy()
	{
		if (m_CommandListHandle)
		{
			m_CommandListHandle->Release();
		}
	}

	void CommandList::Begin()
	{
		m_CommandListHandle->open();

		if (m_Params.TargetSwapChain)
		{
			m_TargetFramebuffer = Application::Get().GetWindow().GetSwapChain()->GetCurrentFramebuffer();
		}
	}

	void CommandList::End()
	{
		m_CommandListHandle->close();
		GraphicsContext::GetDeviceHandle()->executeCommandList(m_CommandListHandle);

		m_TargetFramebuffer = nullptr; // Reset target framebuffer after execution
	}

	void CommandList::Clear()
	{
		nvrhi::utils::ClearColorAttachment(m_CommandListHandle, m_TargetFramebuffer->m_FramebufferHandle, 0, nvrhi::Color(0.3f));
	}

	void CommandList::Draw(Pipeline* pipeline, uint32_t vertexCount, uint32_t instanceCount)
	{
		// Set the graphics state: pipeline, framebuffer, viewport, bindings.
		auto& graphicsState = nvrhi::GraphicsState()
			.setPipeline(static_cast<NvrhiPipeline*>(pipeline)->m_GraphicsPipelineHandle)
			.setFramebuffer(m_TargetFramebuffer->m_FramebufferHandle)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(m_TargetFramebuffer->m_Viewport));

		m_CommandListHandle->setGraphicsState(graphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(vertexCount) // Number of vertices to draw
			.setInstanceCount(instanceCount); // Number of instances to draw

		m_CommandListHandle->draw(drawArguments);
	}

	void CommandList::Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount, uint32_t instanceCount)
	{
		DE_CORE_ASSERT(vertexBuffer, "Vertex buffer is null.");
		DE_CORE_ASSERT(vertexBuffer->IsType(BufferType::VertexBuffer), "Vertex buffer, must be of type BufferType::VertexBuffer");

		const nvrhi::VertexBufferBinding vertexBufferBinding = nvrhi::VertexBufferBinding()
			.setBuffer(static_cast<NvrhiGraphicsBuffer*>(vertexBuffer)->m_BufferHandle)
			.setOffset(0)
			.setSlot(0);

		// Set the graphics state: pipeline, framebuffer, viewport, bindings.
		auto& graphicsState = nvrhi::GraphicsState()
			.addVertexBuffer(vertexBufferBinding)
			.setPipeline(static_cast<NvrhiPipeline*>(pipeline)->m_GraphicsPipelineHandle)
			.setFramebuffer(m_TargetFramebuffer->m_FramebufferHandle)
			.addVertexBuffer(vertexBufferBinding)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(m_TargetFramebuffer->m_Viewport));

		m_CommandListHandle->setGraphicsState(graphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(vertexCount) // Number of vertices to draw
			.setInstanceCount(instanceCount); // Number of instances to draw

		m_CommandListHandle->draw(drawArguments);
	}

	void CommandList::DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer)
	{
		DE_CORE_ASSERT(vertexBuffer, "Vertex buffer is null.");
		DE_CORE_ASSERT(vertexBuffer->IsType(BufferType::VertexBuffer), "Vertex buffer, must be of type BufferType::VertexBuffer");

		DE_CORE_ASSERT(indexBuffer, "Index buffer is null.");
		DE_CORE_ASSERT(indexBuffer->IsType(BufferType::IndexBuffer), "Vertex buffer, must be of type BufferType::IndexBuffer");

		const nvrhi::VertexBufferBinding vertexBufferBinding = nvrhi::VertexBufferBinding()
			.setBuffer(static_cast<NvrhiGraphicsBuffer*>(vertexBuffer)->m_BufferHandle)
			.setOffset(0)
			.setSlot(0);

		const nvrhi::IndexBufferBinding indexBufferBinding = nvrhi::IndexBufferBinding()
			.setBuffer(static_cast<NvrhiGraphicsBuffer*>(indexBuffer)->m_BufferHandle)
			.setOffset(0)
			.setFormat(nvrhi::Format::R16_UINT); // Assuming 16-bit indices

		// Set the graphics state: pipeline, framebuffer, viewport, bindings.
		auto& graphicsState = nvrhi::GraphicsState()
			.addVertexBuffer(vertexBufferBinding)
			.setIndexBuffer(indexBufferBinding)
			.setPipeline(static_cast<NvrhiPipeline*>(pipeline)->m_GraphicsPipelineHandle)
			.setFramebuffer(m_TargetFramebuffer->m_FramebufferHandle)
			.addVertexBuffer(vertexBufferBinding)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(m_TargetFramebuffer->m_Viewport));

		if (static_cast<NvrhiPipeline*>(pipeline)->m_BindingSetHandle)
		{
			graphicsState.addBindingSet(static_cast<NvrhiPipeline*>(pipeline)->m_BindingSetHandle);
		}

		m_CommandListHandle->setGraphicsState(graphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(indexBuffer->GetByteSize() / sizeof(uint16_t)) // Number of vertices to draw
			.setInstanceCount(1); // Number of instances to draw

		m_CommandListHandle->drawIndexed(drawArguments);
	}

	void CommandList::DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer)
	{
		DE_CORE_ASSERT(vertexBuffer, "Vertex buffer is null.");
		DE_CORE_ASSERT(vertexBuffer->IsType(BufferType::VertexBuffer), "Vertex buffer, must be of type BufferType::VertexBuffer");

		DE_CORE_ASSERT(indexBuffer, "Index buffer is null.");
		DE_CORE_ASSERT(indexBuffer->IsType(BufferType::IndexBuffer), "Vertex buffer, must be of type BufferType::IndexBuffer");

		DE_CORE_ASSERT(uniformBuffer, "Uniform buffer is null.");
		DE_CORE_ASSERT(uniformBuffer->IsType(BufferType::UniformBuffer), "Uniform buffer, must be of type BufferType::UniformBuffer");

		m_CommandListHandle->writeBuffer(static_cast<NvrhiGraphicsBuffer*>(uniformBuffer)->m_BufferHandle, uniformBuffer->GetData(), uniformBuffer->GetByteSize());

		const nvrhi::VertexBufferBinding vertexBufferBinding = nvrhi::VertexBufferBinding()
			.setBuffer(static_cast<NvrhiGraphicsBuffer*>(vertexBuffer)->m_BufferHandle)
			.setOffset(0)
			.setSlot(0);

		const nvrhi::IndexBufferBinding indexBufferBinding = nvrhi::IndexBufferBinding()
			.setBuffer(static_cast<NvrhiGraphicsBuffer*>(indexBuffer)->m_BufferHandle)
			.setOffset(0)
			.setFormat(nvrhi::Format::R16_UINT); // Assuming 16-bit indices

		// Set the graphics state: pipeline, framebuffer, viewport, bindings.
		auto& graphicsState = nvrhi::GraphicsState()
			.addVertexBuffer(vertexBufferBinding)
			.setIndexBuffer(indexBufferBinding)
			.setPipeline(static_cast<NvrhiPipeline*>(pipeline)->m_GraphicsPipelineHandle)
			.setFramebuffer(m_TargetFramebuffer->m_FramebufferHandle)
			.addVertexBuffer(vertexBufferBinding)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(m_TargetFramebuffer->m_Viewport));

		if (static_cast<NvrhiPipeline*>(pipeline)->m_BindingSetHandle)
		{
			graphicsState.addBindingSet(static_cast<NvrhiPipeline*>(pipeline)->m_BindingSetHandle);
		}

		m_CommandListHandle->setGraphicsState(graphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(indexBuffer->GetIndexCount()) // Number of vertices to draw
			.setInstanceCount(1); // Number of instances to draw

		m_CommandListHandle->drawIndexed(drawArguments);
	}

}

