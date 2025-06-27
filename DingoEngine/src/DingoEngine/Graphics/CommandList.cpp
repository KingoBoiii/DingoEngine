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

	void CommandList::Begin(Pipeline* pipeline)
	{
		m_CommandListHandle->open();

		// Set the graphics state: pipeline, framebuffer, viewport, bindings.
		auto& graphicsState = nvrhi::GraphicsState()
			.setPipeline(pipeline->m_GraphicsPipelineHandle)
			.setFramebuffer(pipeline->m_Framebuffer->m_FramebufferHandle)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(nvrhi::Viewport(1600, 900)));

		m_CommandListHandle->setGraphicsState(graphicsState);

		//nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
		//	.setVertexCount(3) // Number of vertices to draw
		//	.setInstanceCount(1) // Number of instances to draw
		//	.setStartVertexLocation(0) // Starting vertex index
		//	.setStartIndexLocation(0); // Starting instance index

		//m_CommandListHandle->draw(drawArguments); // Draw a triangle (3 vertices starting from index 0)

		//m_CommandListHandle->setTextureState(
		//	pipeline->m_Framebuffer->m_Texture,
		//	{},
		//	nvrhi::ResourceStates::Present);

	}

	void CommandList::End()
	{
		m_CommandListHandle->close();
		GraphicsContext::GetDeviceHandle()->executeCommandList(m_CommandListHandle);
		m_CommandListHandle->Release(); // Release the command list handle after execution
	}

	void CommandList::Clear(Framebuffer* framebuffer)
	{
		nvrhi::utils::ClearColorAttachment(m_CommandListHandle, framebuffer->m_FramebufferHandle, 0, nvrhi::Color(0.0f));
	}

}

