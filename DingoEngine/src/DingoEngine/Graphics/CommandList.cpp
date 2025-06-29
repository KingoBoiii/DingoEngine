#include "depch.h"
#include "DingoEngine/Graphics/CommandList.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include <nvrhi/utils.h> // for ClearColorAttachment

namespace DingoEngine
{

	CommandList* CommandList::Create()
	{
		return new CommandList();
	}

	void CommandList::Initialize()
	{
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

	void CommandList::Begin(Framebuffer* framebuffer, Pipeline* pipeline, VertexBuffer* vertexBuffer, IndexBuffer* indexBuffer)
	{
		m_CommandListHandle->open();

		m_CommandListHandle->writeBuffer(vertexBuffer->m_BufferHandle, vertexBuffer->m_Data, vertexBuffer->m_Size);
		m_CommandListHandle->writeBuffer(indexBuffer->m_BufferHandle, indexBuffer->m_Indices, sizeof(uint16_t) * indexBuffer->m_Count);

		nvrhi::utils::ClearColorAttachment(m_CommandListHandle, framebuffer->m_FramebufferHandle, 0, nvrhi::Color(0.3f));

		const nvrhi::VertexBufferBinding vertexBufferBinding = nvrhi::VertexBufferBinding()
			.setBuffer(vertexBuffer->m_BufferHandle)
			.setOffset(0)
			.setSlot(0);

		const nvrhi::IndexBufferBinding indexBufferBinding = nvrhi::IndexBufferBinding()
			.setBuffer(indexBuffer->m_BufferHandle)
			.setOffset(0)
			.setFormat(nvrhi::Format::R16_UINT); // Assuming 16-bit indices

		// Set the graphics state: pipeline, framebuffer, viewport, bindings.
		auto& graphicsState = nvrhi::GraphicsState()
			.addVertexBuffer(vertexBufferBinding)
			.setIndexBuffer(indexBufferBinding)
			.setPipeline(pipeline->m_GraphicsPipelineHandle)
			.setFramebuffer(framebuffer->m_FramebufferHandle)
			.addVertexBuffer(vertexBufferBinding)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->m_Viewport));

		m_CommandListHandle->setGraphicsState(graphicsState);
	}

	void CommandList::Begin(Framebuffer* framebuffer, Pipeline* pipeline, VertexBuffer* vertexBuffer)
	{
		m_CommandListHandle->open();

		m_CommandListHandle->writeBuffer(vertexBuffer->m_BufferHandle, vertexBuffer->m_Data, vertexBuffer->m_Size);

		nvrhi::utils::ClearColorAttachment(m_CommandListHandle, framebuffer->m_FramebufferHandle, 0, nvrhi::Color(0.3f));

		const nvrhi::VertexBufferBinding vertexBufferBinding = nvrhi::VertexBufferBinding()
			.setBuffer(vertexBuffer->m_BufferHandle)
			.setOffset(0)
			.setSlot(0); // Assuming slot 0 for the vertex buffer
			//.setStride(pipeline->m_VertexLayout.Stride);

		// Set the graphics state: pipeline, framebuffer, viewport, bindings.
		auto& graphicsState = nvrhi::GraphicsState()
			.addVertexBuffer(vertexBufferBinding)
			.setPipeline(pipeline->m_GraphicsPipelineHandle)
			.setFramebuffer(framebuffer->m_FramebufferHandle)
			.addVertexBuffer(vertexBufferBinding)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->m_Viewport));

		m_CommandListHandle->setGraphicsState(graphicsState);
	}

	void CommandList::Begin(Framebuffer* framebuffer, Pipeline* pipeline)
	{
		m_CommandListHandle->open();

		nvrhi::utils::ClearColorAttachment(m_CommandListHandle, framebuffer->m_FramebufferHandle, 0, nvrhi::Color(0.3f));

		// Set the graphics state: pipeline, framebuffer, viewport, bindings.
		auto& graphicsState = nvrhi::GraphicsState()
			.setPipeline(pipeline->m_GraphicsPipelineHandle)
			.setFramebuffer(framebuffer->m_FramebufferHandle)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->m_Viewport));

		m_CommandListHandle->setGraphicsState(graphicsState);
	}

	void CommandList::End()
	{
		m_CommandListHandle->close();
		GraphicsContext::GetDeviceHandle()->executeCommandList(m_CommandListHandle);
	}

	void CommandList::Clear(Framebuffer* framebuffer)
	{
		nvrhi::utils::ClearColorAttachment(m_CommandListHandle, framebuffer->m_FramebufferHandle, 0, nvrhi::Color(0.0f));
	}

	void CommandList::Draw()
	{
		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(3) // Number of vertices to draw
			.setInstanceCount(1); // Number of instances to draw
			//.setStartIndexLocation(0) // Starting instance index
			//.setStartVertexLocation(0); // Starting vertex index

		m_CommandListHandle->draw(drawArguments); // Draw a triangle (3 vertices starting from index 0)
	}

	void CommandList::DrawIndexed(IndexBuffer* indexBuffer)
	{
		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(indexBuffer->m_Count) // Number of vertices to draw
			.setInstanceCount(1); // Number of instances to draw
		//.setStartIndexLocation(0) // Starting instance index
		//.setStartVertexLocation(0); // Starting vertex index

		m_CommandListHandle->drawIndexed(drawArguments); // Draw a triangle (3 vertices starting from index 0)
	}

}

