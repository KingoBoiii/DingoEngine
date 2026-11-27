#include "depch.h"
#include "NvrhiCommandList.h"
#include "NvrhiFramebuffer.h"
#include "NvrhiPipeline.h"
#include "NvrhiGraphicsBuffer.h"
#include "NvrhiGraphicsContext.h"
#include "NvrhiRenderPass.h"
#include "NvrhiComputePass.h"
#include "NvrhiShader.h"
#include "NvrhiTexture.h"

#include "DingoEngine/Core/Application.h"

#include <nvrhi/utils.h>

namespace Dingo
{

	static_assert(Shader::k_MaxPushConstantSize == nvrhi::c_MaxPushConstantSize);

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
		m_LastDrawFramebuffer = nullptr;
		m_UploadedSinceDraw = false;
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

	void NvrhiCommandList::Clear(Framebuffer* framebuffer, uint32_t attachmentIndex, const glm::vec4& clearColor)
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

		// A depth-only framebuffer (a shadow map) has no colour to clear.
		const auto& colorAttachments = nvrhiFB->m_FramebufferHandle->getDesc().colorAttachments;
		if (attachmentIndex < colorAttachments.size() && colorAttachments[attachmentIndex].texture)
		{
			nvrhi::utils::ClearColorAttachment(m_CommandListHandle, nvrhiFB->m_FramebufferHandle, attachmentIndex, { clearColor.r, clearColor.g, clearColor.b, clearColor.a });

			// NVRHI's Vulkan clear doesn't make the command buffer hold the texture, so a target only
			// cleared in a frame could be freed while the GPU clears it. setTextureState does, and
			// re-requiring the state the clear left it in records no barrier on any backend.
			const nvrhi::FramebufferAttachment& color = colorAttachments[attachmentIndex];
			const nvrhi::TextureSubresourceSet cleared = color.subresources.resolve(color.texture->getDesc(), false);
			m_CommandListHandle->setTextureState(color.texture, cleared,
				m_CommandListHandle->getTextureSubresourceState(color.texture, cleared.baseArraySlice, cleared.baseMipLevel));
		}

		if (hasDepth)
			nvrhi::utils::ClearDepthStencilAttachment(m_CommandListHandle, nvrhiFB->m_FramebufferHandle, k_DepthClearValue, 0);

		m_LastDrawFramebuffer = nullptr;
	}

	void NvrhiCommandList::ClearTexture(Texture* texture, const glm::vec4& value)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before clearing a texture.");
		DE_CORE_ASSERT(texture, "Texture is null.");
		DE_CORE_ASSERT(texture->GetParams().IsStorage || texture->GetParams().IsRenderTarget, "ClearTexture takes a storage or render-target texture.");

		nvrhi::ITexture* handle = static_cast<NvrhiTexture*>(texture)->m_Handle;
		m_CommandListHandle->clearTextureFloat(handle, nvrhi::AllSubresources, nvrhi::Color(value.r, value.g, value.b, value.a));

		// As in UploadTexture: a binding set kept bound across the clear would otherwise sample a copy
		// destination.
		const nvrhi::TextureDesc& desc = handle->getDesc();
		if (desc.initialState != nvrhi::ResourceStates::Unknown)
		{
			m_CommandListHandle->setTextureState(handle, nvrhi::AllSubresources, desc.initialState);
			m_CommandListHandle->commitBarriers();
		}
		m_LastDrawFramebuffer = nullptr;
	}

	void NvrhiCommandList::UploadBuffer(GraphicsBuffer* buffer, const void* data, uint64_t size, uint64_t offset)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before uploading buffer.");
		DE_CORE_ASSERT(buffer, "Uniform buffer is null.");

		static_cast<NvrhiGraphicsBuffer*>(buffer)->Write(m_CommandListHandle, data, size, offset);
		m_UploadedSinceDraw = m_LastDrawFramebuffer != nullptr;
	}

	void NvrhiCommandList::UploadKeptData(GraphicsBuffer* buffer)
	{
		NvrhiGraphicsBuffer* nvrhiBuffer = static_cast<NvrhiGraphicsBuffer*>(buffer);
		UploadBuffer(buffer, nvrhiBuffer->m_Data, nvrhiBuffer->m_DataSize, nvrhiBuffer->m_DataOffset);
	}

	void NvrhiCommandList::UploadTexture(Texture* texture, const void* data, uint32_t mip, uint32_t layer, uint64_t rowPitch, uint64_t depthPitch)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before uploading a texture.");
		DE_CORE_ASSERT(texture, "Texture is null.");

		nvrhi::ITexture* handle = static_cast<NvrhiTexture*>(texture)->m_Handle;
		m_CommandListHandle->writeTexture(handle, layer, mip, data, rowPitch, depthPitch);
		m_UploadedSinceDraw = m_LastDrawFramebuffer != nullptr;

		// NVRHI requires a binding set's states only when the bound sets change, so a draw that keeps
		// the set bound before the upload would sample the texture as a copy destination. Committed at
		// once, as NvrhiGraphicsBuffer::Write does, so the state is never ORed into a later one.
		const nvrhi::TextureDesc& desc = handle->getDesc();
		if (desc.initialState != nvrhi::ResourceStates::Unknown)
		{
			m_CommandListHandle->setTextureState(handle, nvrhi::TextureSubresourceSet(mip, 1, layer, 1), desc.initialState);
			m_CommandListHandle->commitBarriers();
		}
	}

	void NvrhiCommandList::CopyTexture(Texture* dst, Texture* src)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before copying a texture.");
		DE_CORE_ASSERT(dst && src, "Texture is null.");

		nvrhi::ITexture* dstHandle = static_cast<NvrhiTexture*>(dst)->m_Handle;
		nvrhi::ITexture* srcHandle = static_cast<NvrhiTexture*>(src)->m_Handle;
		const nvrhi::TextureDesc& dstDesc = dstHandle->getDesc();
		const nvrhi::TextureDesc& srcDesc = srcHandle->getDesc();
		DE_CORE_ASSERT(dstDesc.width == srcDesc.width && dstDesc.height == srcDesc.height && dstDesc.depth == srcDesc.depth &&
			dstDesc.arraySize == srcDesc.arraySize && dstDesc.mipLevels == srcDesc.mipLevels && dstDesc.format == srcDesc.format,
			"CopyTexture needs textures of the same size, layers, mips and format.");

		m_CommandListHandle->setTextureState(srcHandle, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
		m_CommandListHandle->setTextureState(dstHandle, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
		m_CommandListHandle->commitBarriers();

		for (uint32_t layer = 0; layer < dstDesc.arraySize; ++layer)
		{
			for (uint32_t mip = 0; mip < dstDesc.mipLevels; ++mip)
			{
				nvrhi::TextureSlice slice;
				slice.mipLevel = mip;
				slice.arraySlice = layer;
				m_CommandListHandle->copyTexture(dstHandle, slice, srcHandle, slice);
			}
		}
		m_UploadedSinceDraw = m_LastDrawFramebuffer != nullptr;

		// As UploadTexture: a stable binding set samples dst, and NVRHI only re-requires a set's states
		// when the bound sets change. src goes back too, so a later dispatch into it starts from its own state.
		if (dstDesc.initialState != nvrhi::ResourceStates::Unknown)
			m_CommandListHandle->setTextureState(dstHandle, nvrhi::AllSubresources, dstDesc.initialState);
		if (srcDesc.initialState != nvrhi::ResourceStates::Unknown)
			m_CommandListHandle->setTextureState(srcHandle, nvrhi::AllSubresources, srcDesc.initialState);
		m_CommandListHandle->commitBarriers();
	}

	void NvrhiCommandList::CopyBuffer(GraphicsBuffer* dst, GraphicsBuffer* src, uint64_t size)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before copying a buffer.");
		DE_CORE_ASSERT(dst && src, "Buffer is null.");

		nvrhi::IBuffer* dstHandle = static_cast<NvrhiGraphicsBuffer*>(dst)->m_BufferHandle;
		nvrhi::IBuffer* srcHandle = static_cast<NvrhiGraphicsBuffer*>(src)->m_BufferHandle;
		if (size > dstHandle->getDesc().byteSize || size > srcHandle->getDesc().byteSize)
		{
			DE_CORE_ERROR("CopyBuffer: {} bytes do not fit both buffers ({} and {}).", size, dstHandle->getDesc().byteSize, srcHandle->getDesc().byteSize);
			return;
		}
		m_CommandListHandle->copyBuffer(dstHandle, 0, srcHandle, 0, size);
		m_UploadedSinceDraw = m_LastDrawFramebuffer != nullptr;

		const nvrhi::BufferDesc& dstDesc = dstHandle->getDesc();
		const nvrhi::BufferDesc& srcDesc = srcHandle->getDesc();
		if (!dstDesc.isVolatile && dstDesc.initialState != nvrhi::ResourceStates::Unknown)
			m_CommandListHandle->setBufferState(dstHandle, dstDesc.initialState);
		if (!srcDesc.isVolatile && srcDesc.initialState != nvrhi::ResourceStates::Unknown)
			m_CommandListHandle->setBufferState(srcHandle, srcDesc.initialState);
		m_CommandListHandle->commitBarriers();
	}

	void NvrhiCommandList::SetFramebuffer(Framebuffer* framebuffer)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before setting pipeline.");
		DE_CORE_ASSERT(framebuffer, "Framebuffer is null.");

		m_GraphicsState.setFramebuffer(static_cast<NvrhiFramebuffer*>(framebuffer)->m_FramebufferHandle)
			.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(static_cast<NvrhiFramebuffer*>(framebuffer)->m_Viewport));
	}

	void NvrhiCommandList::SetViewport(const Viewport& viewport)
	{
		const nvrhi::Viewport nvrhiViewport(viewport.X, viewport.X + viewport.Width, viewport.Y, viewport.Y + viewport.Height, 0.0f, 1.0f);
		m_GraphicsState.setViewport(nvrhi::ViewportState().addViewportAndScissorRect(nvrhiViewport));
	}

	bool NvrhiCommandList::SetPipeline(Pipeline* pipeline)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before setting pipeline.");
		DE_CORE_ASSERT(pipeline, "Pipeline is null.");

		m_GraphicsState = nvrhi::GraphicsState();
		m_RenderPassStorageItems = nullptr;
		m_PushConstantSize = 0;
		m_PushConstants.fill(0);

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
		m_PushConstantSize = shader ? static_cast<NvrhiShader*>(shader)->m_PushConstantSize : 0;

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
		if (!nvrhiRenderPass->m_StorageItems.empty())
			m_RenderPassStorageItems = &nvrhiRenderPass->m_StorageItems;

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

		const nvrhi::Format nvrhiFormat = (indexBuffer->GetIndexFormat() == IndexFormat::Uint32)
			? nvrhi::Format::R32_UINT
			: nvrhi::Format::R16_UINT;

		const nvrhi::IndexBufferBinding indexBufferBinding = nvrhi::IndexBufferBinding()
			.setBuffer(static_cast<NvrhiGraphicsBuffer*>(indexBuffer)->m_BufferHandle)
			.setOffset(0)
			.setFormat(nvrhiFormat);

		m_GraphicsState.setIndexBuffer(indexBufferBinding);
	}

	void NvrhiCommandList::SetPushConstants(const void* data, uint32_t size)
	{
		DE_CORE_ASSERT(data, "Push-constant data is null.");
		std::memcpy(m_PushConstants.data(), data, (std::min)(static_cast<size_t>(size), m_PushConstants.size()));
	}

	void NvrhiCommandList::BeginDraw()
	{
		if (m_RenderPassStorageItems)
			RequireStorageStates(*m_RenderPassStorageItems);
		m_CommandListHandle->setGraphicsState(m_GraphicsState);
		if (m_PushConstantSize)
			m_CommandListHandle->setPushConstants(m_PushConstants.data(), m_PushConstantSize);

		if (m_UploadedSinceDraw && m_GraphicsState.framebuffer == m_LastDrawFramebuffer)
			++m_UploadPassBreaks;
		m_UploadedSinceDraw = false;
		m_LastDrawFramebuffer = m_GraphicsState.framebuffer;
	}

	void NvrhiCommandList::Draw(uint32_t vertexCount, uint32_t instanceCount)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before drawing.");

		// The backends dereference the pipeline unchecked, so an unset one is an access
		// violation rather than a dropped draw. SetPipeline already logged the reason.
		if (!m_GraphicsState.pipeline)
			return;

		BeginDraw();

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(vertexCount) // Number of vertices to draw
			.setInstanceCount(instanceCount); // Number of instances to draw

		m_CommandListHandle->draw(drawArguments);
	}

	void NvrhiCommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before drawing.");

		if (!m_GraphicsState.pipeline)
			return;

		BeginDraw();

		nvrhi::DrawArguments drawArguments = nvrhi::DrawArguments()
			.setVertexCount(indexCount) // Number of vertices to draw
			.setInstanceCount(instanceCount) // Number of instances to draw
			.setStartIndexLocation(firstIndex);

		m_CommandListHandle->drawIndexed(drawArguments);
	}

	bool NvrhiCommandList::Dispatch(ComputePass* pass, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
	{
		DE_CORE_ASSERT(m_HasBegun, "Command list must be begun before dispatching.");
		DE_CORE_ASSERT(pass, "Compute pass is null.");

		if (groupsX == 0 || groupsY == 0 || groupsZ == 0)
			return true;
		NvrhiComputePass* nvrhiPass = static_cast<NvrhiComputePass*>(pass);
		if (!nvrhiPass->Prepare())
			return false;

		nvrhi::ComputeState state = nvrhi::ComputeState().setPipeline(nvrhiPass->GetPipelineHandle());
		if (nvrhiPass->GetBindingSetHandle())
			state.addBindingSet(nvrhiPass->GetBindingSetHandle());
		RequireStorageStates(nvrhiPass->GetStorageItems());
		m_CommandListHandle->setComputeState(state);
		const NvrhiShader* shader = static_cast<const NvrhiShader*>(pass->GetParams().Shader);
		if (shader->m_PushConstantSize)
			m_CommandListHandle->setPushConstants(nvrhiPass->GetPushConstantData(), shader->m_PushConstantSize);
		m_CommandListHandle->dispatch(groupsX, groupsY, groupsZ);
		m_LastDrawFramebuffer = nullptr;
		return true;
	}

	void NvrhiCommandList::SortBySlot(nvrhi::BindingSetDesc& desc)
	{
		std::stable_sort(desc.bindings.begin(), desc.bindings.end(),
			[](const nvrhi::BindingSetItem& a, const nvrhi::BindingSetItem& b) { return a.slot < b.slot; });
	}

	bool NvrhiCommandList::IsStorageItem(const nvrhi::BindingSetItem& item)
	{
		return item.type == nvrhi::ResourceType::RawBuffer_SRV || item.type == nvrhi::ResourceType::RawBuffer_UAV
			|| item.type == nvrhi::ResourceType::Texture_UAV;
	}

	void NvrhiCommandList::RequireStorageStates(const std::vector<nvrhi::BindingSetItem>& items)
	{
		for (const nvrhi::BindingSetItem& item : items)
		{
			switch (item.type)
			{
				case nvrhi::ResourceType::RawBuffer_SRV:
					m_CommandListHandle->setBufferState(static_cast<nvrhi::IBuffer*>(item.resourceHandle), nvrhi::ResourceStates::ShaderResource);
					break;
				case nvrhi::ResourceType::RawBuffer_UAV:
					m_CommandListHandle->setBufferState(static_cast<nvrhi::IBuffer*>(item.resourceHandle), nvrhi::ResourceStates::UnorderedAccess);
					break;
				case nvrhi::ResourceType::Texture_UAV:
					m_CommandListHandle->setTextureState(static_cast<nvrhi::ITexture*>(item.resourceHandle), item.subresources, nvrhi::ResourceStates::UnorderedAccess);
					break;
				case nvrhi::ResourceType::Texture_SRV:
					m_CommandListHandle->setTextureState(static_cast<nvrhi::ITexture*>(item.resourceHandle), item.subresources, nvrhi::ResourceStates::ShaderResource);
					break;
				default:
					break;
			}
		}
	}

}
