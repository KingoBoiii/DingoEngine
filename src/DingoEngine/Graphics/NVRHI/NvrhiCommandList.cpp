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
		nvrhi::CommandListParameters commandListParameters = nvrhi::CommandListParameters()
			//.setEnableImmediateExecution(false) // Set to true for immediate execution, false for deferred execution
			.setQueueType(nvrhi::CommandQueue::Graphics);

		m_CommandListHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createCommandList(commandListParameters);
	}

	void NvrhiCommandList::Destroy()
	{
		m_CommandListHandle = nullptr;
	}

	void NvrhiCommandList::Begin()
	{
		//m_GraphicsState = nvrhi::GraphicsState();

		m_CommandListHandle->open();

		m_HasBegun = true;
	}

	void NvrhiCommandList::Close()
	{
		m_CommandListHandle->close();
		m_HasBegun = false;
	}

	void NvrhiCommandList::Execute()
	{
		GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->executeCommandList(m_CommandListHandle);
	}

	void NvrhiCommandList::End()
	{
		Close();
		Execute();
	}

	void NvrhiCommandList::Clear(Framebuffer* framebuffer, uint32_t attachmentIndex, const glm::vec3& clearColor)
	{
		DE_CORE_ASSERT(framebuffer, "Framebuffer is null.");
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before clearing framebuffer.");

		auto* nvrhiFB = static_cast<NvrhiFramebuffer*>(framebuffer);

		// NVRHI's clearDepthStencilTexture omits the automatic barrier that clearTexture has,
		// so the depth image never gets transitioned to TransferDstOptimal before the clear.
		// Queue the transition here so the color clear's internal commitBarriers() carries both.
		const bool hasDepth = nvrhiFB->m_DepthTextureHandle != nullptr &&
		                      nvrhiFB->m_FramebufferHandle->getFramebufferInfo().depthFormat != nvrhi::Format::UNKNOWN;
		if (hasDepth)
			m_CommandListHandle->setTextureState(nvrhiFB->m_DepthTextureHandle, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);

		nvrhi::utils::ClearColorAttachment(m_CommandListHandle, nvrhiFB->m_FramebufferHandle, attachmentIndex, { clearColor.r, clearColor.g, clearColor.b, 1.0f });

		if (hasDepth)
			nvrhi::utils::ClearDepthStencilAttachment(m_CommandListHandle, nvrhiFB->m_FramebufferHandle, 1.0f, 0);
	}

	void NvrhiCommandList::UploadBuffer(GraphicsBuffer* buffer, const void* data, uint64_t size, uint64_t offset)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before uploading buffer.");
		DE_CORE_ASSERT(buffer, "Uniform buffer is null.");

		m_CommandListHandle->writeBuffer(static_cast<NvrhiGraphicsBuffer*>(buffer)->m_BufferHandle, data, size, offset);
	}

	void NvrhiCommandList::SetFramebuffer(Framebuffer* framebuffer)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before setting pipeline.");
		DE_CORE_ASSERT(framebuffer, "Framebuffer is null.");

		m_GraphicsState.setFramebuffer(static_cast<NvrhiFramebuffer*>(framebuffer)->m_FramebufferHandle)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(static_cast<NvrhiFramebuffer*>(framebuffer)->m_Viewport));
	}

	bool NvrhiCommandList::SetPipeline(Pipeline* pipeline)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before setting pipeline.");
		DE_CORE_ASSERT(pipeline, "Pipeline is null.");

		m_GraphicsState = nvrhi::GraphicsState();

		if (!pipeline)
			return false;

		NvrhiPipeline* nvrhiPipeline = static_cast<NvrhiPipeline*>(pipeline);

		// Lazily rebuild PSOs whose shader — or whose bound texture — was hot-reloaded
		// since they were created. The texture check matters because the PSO's binding set
		// holds the native handle, which Reinitialize replaces.
		Shader* shader = nvrhiPipeline->GetParams().Shader;
		Texture* texture = nvrhiPipeline->GetParams().Texture;
		const bool shaderReloaded = shader && shader->GetGeneration() != nvrhiPipeline->m_BuiltShaderGeneration;
		const bool textureReloaded = texture && texture->GetGeneration() != nvrhiPipeline->m_BuiltTextureGeneration;

		if (shaderReloaded || textureReloaded)
		{
			nvrhiPipeline->Destroy();
			nvrhiPipeline->Initialize();
		}

		if (!nvrhiPipeline->m_GraphicsPipelineHandle)
		{
			DE_CORE_ERROR("SetPipeline: pipeline '{}' has a null graphics pipeline handle — shader/PSO creation failed. Draw call will be skipped.", nvrhiPipeline->GetParams().DebugName);
			return false;
		}

		// Deliberately no SetFramebuffer(pipeline->GetTargetFramebuffer()) here: that pointer
		// is the framebuffer the pipeline was CREATED against, and for swap-chain pipelines it
		// dies on every resize. Callers must SetFramebuffer() with the current target after this.
		m_GraphicsState.setPipeline(nvrhiPipeline->m_GraphicsPipelineHandle);

		if (nvrhiPipeline->m_BindingSetHandle)
		{
			m_GraphicsState.addBindingSet(nvrhiPipeline->m_BindingSetHandle);
		}

		return true;
	}

	bool NvrhiCommandList::SetRenderPass(RenderPass* renderPass)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before setting render pass.");
		DE_CORE_ASSERT(renderPass, "Render Pass is null.");

		if (!renderPass)
			return false;

		NvrhiRenderPass* nvrhiRenderPass = static_cast<NvrhiRenderPass*>(renderPass);

		if (!SetPipeline(renderPass->GetPipeline()))
			return false;

		// A hot-reloaded shader replaced its binding layout - the baked binding set
		// belongs to the old layout and must be re-baked before it is bound. A reloaded
		// texture is a new native handle, which owners that bake once (Material) never
		// re-set themselves.
		Shader* shader = renderPass->GetPipeline()->GetParams().Shader;
		const bool shaderReloaded = shader && shader->GetGeneration() != nvrhiRenderPass->m_BuiltShaderGeneration;
		const bool texturesReloaded = nvrhiRenderPass->RefreshReloadedTextures();

		if (shaderReloaded || texturesReloaded)
		{
			nvrhiRenderPass->m_Valid = false;
			nvrhiRenderPass->Bake();
		}

		if (!nvrhiRenderPass->m_BindingSetHandle)
		{
			// A pass with nothing to bind is legitimate; one whose bake failed is not, and
			// drawing it would sample whatever the layout expects from unwritten descriptors.
			return nvrhiRenderPass->m_BindingSetDesc.bindings.empty();
		}

		m_GraphicsState.addBindingSet(nvrhiRenderPass->m_BindingSetHandle);

		return true;
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

		const nvrhi::Format nvrhiFormat = (indexBuffer->GetFormat() == GraphicsFormat::Uint32)
			? nvrhi::Format::R32_UINT
			: nvrhi::Format::R16_UINT;

		const nvrhi::IndexBufferBinding indexBufferBinding = nvrhi::IndexBufferBinding()
			.setBuffer(static_cast<NvrhiGraphicsBuffer*>(indexBuffer)->m_BufferHandle)
			.setOffset(0)
			.setFormat(nvrhiFormat);

		m_GraphicsState.setIndexBuffer(indexBufferBinding);
	}

	void NvrhiCommandList::Draw(uint32_t vertexCount, uint32_t instanceCount)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before drawing.");

		// The backends dereference the pipeline unchecked, so an unset one is an access
		// violation rather than a dropped draw. SetPipeline already logged the reason.
		if (!m_GraphicsState.pipeline)
			return;

		m_CommandListHandle->setGraphicsState(m_GraphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(vertexCount) // Number of vertices to draw
			.setInstanceCount(instanceCount); // Number of instances to draw

		m_CommandListHandle->draw(drawArguments);
	}

	void NvrhiCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before drawing.");

		if (!m_GraphicsState.pipeline)
			return;

		m_CommandListHandle->setGraphicsState(m_GraphicsState);

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(indexCount) // Number of vertices to draw
			.setInstanceCount(instanceCount); // Number of instances to draw

		m_CommandListHandle->drawIndexed(drawArguments);
	}

}
