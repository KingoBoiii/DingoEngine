#include "depch.h"
#include "DingoEngine/Graphics/CommandList.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "DingoEngine/Core/Application.h"

#include <nvrhi/utils.h> // for ClearColorAttachment
#include "GraphicsUtils.h"

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
			.setPipeline(pipeline->m_GraphicsPipelineHandle)
			.setFramebuffer(m_TargetFramebuffer->m_FramebufferHandle)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(m_TargetFramebuffer->m_Viewport));

		m_CommandListHandle->setGraphicsState(graphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(vertexCount) // Number of vertices to draw
			.setInstanceCount(instanceCount); // Number of instances to draw

		m_CommandListHandle->draw(drawArguments);
	}

	void CommandList::Draw(Pipeline* pipeline, VertexBuffer* vertexBuffer)
	{
		const nvrhi::VertexBufferBinding vertexBufferBinding = nvrhi::VertexBufferBinding()
			.setBuffer(vertexBuffer->m_BufferHandle)
			.setOffset(0)
			.setSlot(0);

		// Set the graphics state: pipeline, framebuffer, viewport, bindings.
		auto& graphicsState = nvrhi::GraphicsState()
			.addVertexBuffer(vertexBufferBinding)
			.setPipeline(pipeline->m_GraphicsPipelineHandle)
			.setFramebuffer(m_TargetFramebuffer->m_FramebufferHandle)
			.addVertexBuffer(vertexBufferBinding)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(m_TargetFramebuffer->m_Viewport));

		m_CommandListHandle->setGraphicsState(graphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(3) // Number of vertices to draw
			.setInstanceCount(1); // Number of instances to draw

		m_CommandListHandle->draw(drawArguments);
	}

	void CommandList::DrawIndexed(Pipeline* pipeline, VertexBuffer* vertexBuffer, IndexBuffer* indexBuffer)
	{
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
			.setFramebuffer(m_TargetFramebuffer->m_FramebufferHandle)
			.addVertexBuffer(vertexBufferBinding)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(m_TargetFramebuffer->m_Viewport));

		if (pipeline->m_BindingSetHandle)
		{
			graphicsState.addBindingSet(pipeline->m_BindingSetHandle);
		}

		m_CommandListHandle->setGraphicsState(graphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(indexBuffer->m_Count) // Number of vertices to draw
			.setInstanceCount(1); // Number of instances to draw

		m_CommandListHandle->drawIndexed(drawArguments);
	}

	void CommandList::DrawIndexed(Pipeline* pipeline, VertexBuffer* vertexBuffer, IndexBuffer* indexBuffer, UniformBuffer* uniformBuffer)
	{
		Utils::WriteBuffer(m_CommandListHandle, uniformBuffer->m_BufferHandle, uniformBuffer->m_Data, uniformBuffer->m_Size);

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
			.setFramebuffer(m_TargetFramebuffer->m_FramebufferHandle)
			.addVertexBuffer(vertexBufferBinding)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(m_TargetFramebuffer->m_Viewport));

		if (pipeline->m_BindingSetHandle)
		{
			graphicsState.addBindingSet(pipeline->m_BindingSetHandle);
		}

		m_CommandListHandle->setGraphicsState(graphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(indexBuffer->m_Count) // Number of vertices to draw
			.setInstanceCount(1); // Number of instances to draw

		m_CommandListHandle->drawIndexed(drawArguments);
	}

}

