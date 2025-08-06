#include "depch.h"
#include "NvrhiCommandList.h"
#include "NvrhiFramebuffer.h"
#include "NvrhiPipeline.h"
#include "NvrhiGraphicsBuffer.h"
#include "NvrhiGraphicsContext.h"
#include "NvrhiRenderPass.h"

#include "DingoEngine/Core/Application.h"

#include <nvrhi/utils.h>

namespace Dingo
{

	void NvrhiCommandList::Initialize()
	{
		DE_CORE_ASSERT(m_Params.TargetSwapChain || m_Params.TargetFramebuffer, "CommandList must target a swap chain - does not support otherwise atm.");

		nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
			//.setEnableImmediateExecution(false) // Set to true for immediate execution, false for deferred execution
			.setQueueType(nvrhi::CommandQueue::Graphics);

		m_CommandListHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createCommandList(commandListParameters);
	}

	void NvrhiCommandList::Destroy()
	{
		if (m_CommandListHandle)
		{
			m_CommandListHandle->Release();
		}
	}

	void NvrhiCommandList::Begin()
	{
		m_CommandListHandle->open();

		m_HasBegun = true;
	}

	void NvrhiCommandList::Begin(Framebuffer* framebuffer)
	{
		m_CommandListHandle->open();

		m_GraphicsState = nvrhi::GraphicsState()
			.setFramebuffer(static_cast<NvrhiFramebuffer*>(framebuffer)->m_FramebufferHandle)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(static_cast<NvrhiFramebuffer*>(framebuffer)->m_Viewport));

		m_HasBegun = true;
	}

	void NvrhiCommandList::End()
	{
		m_CommandListHandle->close();
		GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->executeCommandList(m_CommandListHandle);

		m_TargetFramebuffer = nullptr; // Reset target framebuffer after execution
		m_HasBegun = false;
	}

	void NvrhiCommandList::Clear(Framebuffer* framebuffer, uint32_t attachmentIndex, const glm::vec3& clearColor)
	{
		DE_CORE_ASSERT(framebuffer, "Framebuffer is null.");
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before clearing framebuffer.");

		nvrhi::utils::ClearColorAttachment(m_CommandListHandle, static_cast<NvrhiFramebuffer*>(framebuffer)->m_FramebufferHandle, attachmentIndex, { clearColor.r, clearColor.g, clearColor.b, 1.0f });
	}

	void NvrhiCommandList::UploadBuffer(GraphicsBuffer* buffer, const void* data, uint64_t size, uint64_t offset)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before uploading buffer.");

		DE_CORE_ASSERT(buffer, "Uniform buffer is null.");

		m_CommandListHandle->writeBuffer(static_cast<NvrhiGraphicsBuffer*>(buffer)->m_BufferHandle, data, size, offset);
	}

	void NvrhiCommandList::SetPipeline(Pipeline* pipeline)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before setting pipeline.");

		DE_CORE_ASSERT(pipeline, "Pipeline is null.");

		NvrhiPipeline* nvrhiPipeline = static_cast<NvrhiPipeline*>(pipeline);

		m_GraphicsState = nvrhi::GraphicsState()
			.setFramebuffer(static_cast<NvrhiFramebuffer*>(nvrhiPipeline->GetTargetFramebuffer())->m_FramebufferHandle)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(static_cast<NvrhiFramebuffer*>(nvrhiPipeline->GetTargetFramebuffer())->m_Viewport));

		m_GraphicsState.setPipeline(nvrhiPipeline->m_GraphicsPipelineHandle);

		if (nvrhiPipeline->m_BindingSetHandle)
		{
			m_GraphicsState.addBindingSet(nvrhiPipeline->m_BindingSetHandle);
		}
	}

	void NvrhiCommandList::SetRenderPass(RenderPass* renderPass)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before setting render pass.");
		DE_CORE_ASSERT(renderPass, "Render Pass is null.");

		NvrhiRenderPass* nvrhiRenderPass = static_cast<NvrhiRenderPass*>(renderPass);
		NvrhiPipeline* nvrhiPipeline = static_cast<NvrhiPipeline*>(renderPass->GetPipeline());

		m_GraphicsState = nvrhi::GraphicsState()
			.setFramebuffer(static_cast<NvrhiFramebuffer*>(nvrhiPipeline->GetTargetFramebuffer())->m_FramebufferHandle)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(static_cast<NvrhiFramebuffer*>(nvrhiPipeline->GetTargetFramebuffer())->m_Viewport));

		m_GraphicsState.setPipeline(nvrhiPipeline->m_GraphicsPipelineHandle);

		if (nvrhiRenderPass->m_BindingSetHandle)
		{
			m_GraphicsState.addBindingSet(nvrhiRenderPass->m_BindingSetHandle);
		}
	}

	void NvrhiCommandList::AddVertexBuffer(GraphicsBuffer* vertexBuffer, uint32_t slot, uint64_t offset)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before adding vertex buffer.");

		DE_CORE_ASSERT(vertexBuffer, "Vertex buffer is null.");
		DE_CORE_ASSERT(vertexBuffer->IsType(BufferType::VertexBuffer), "Graphics buffer, must be of type BufferType::VertexBuffer");

		const nvrhi::VertexBufferBinding vertexBufferBinding = nvrhi::VertexBufferBinding()
			.setBuffer(static_cast<NvrhiGraphicsBuffer*>(vertexBuffer)->m_BufferHandle)
			.setOffset(offset)
			.setSlot(slot);

		m_GraphicsState.addVertexBuffer(vertexBufferBinding);
	}

	void NvrhiCommandList::SetIndexBuffer(GraphicsBuffer* indexBuffer, uint64_t offset)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before setting index buffer.");

		DE_CORE_ASSERT(indexBuffer, "Index buffer is null.");
		DE_CORE_ASSERT(indexBuffer->IsType(BufferType::IndexBuffer), "Graphics buffer, must be of type BufferType::IndexBuffer");

		const nvrhi::IndexBufferBinding indexBufferBinding = nvrhi::IndexBufferBinding()
			.setBuffer(static_cast<NvrhiGraphicsBuffer*>(indexBuffer)->m_BufferHandle)
			.setOffset(0)
			.setFormat(nvrhi::Format::R16_UINT); // Assuming 16-bit indices

		m_GraphicsState.setIndexBuffer(indexBufferBinding);
	}

	void NvrhiCommandList::Draw(uint32_t vertexCount, uint32_t instanceCount)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before drawing.");

		m_CommandListHandle->setGraphicsState(m_GraphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(vertexCount) // Number of vertices to draw
			.setInstanceCount(instanceCount); // Number of instances to draw

		m_CommandListHandle->draw(drawArguments);
	}

	void NvrhiCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before drawing.");

		m_CommandListHandle->setGraphicsState(m_GraphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(indexCount) // Number of vertices to draw
			.setInstanceCount(instanceCount); // Number of instances to draw

		m_CommandListHandle->drawIndexed(drawArguments);
	}

}
