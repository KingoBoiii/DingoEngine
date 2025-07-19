#include "depch.h"
#include "ImGuiRenderer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/SwapChain.h"
#include "DingoEngine/Graphics/NVRHI/NvrhiShader.h"
#include "DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/NvrhiFramebuffer.h"

#include <nvrhi/utils.h>

namespace Dingo
{

	namespace Utils
	{

		inline static void UploadTextureData(nvrhi::IDevice* device, nvrhi::ITexture* texture, const uint8_t* data, size_t rowPitch)
		{
			nvrhi::CommandListHandle commandList = device->createCommandList();

			commandList->open();

			commandList->beginTrackingTextureState(texture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);

			commandList->writeTexture(texture, 0, 0, data, rowPitch);

			commandList->setPermanentTextureState(texture, nvrhi::ResourceStates::ShaderResource);
			commandList->commitBarriers();

			commandList->close();
			device->executeCommandList(commandList);
		}

	}

	static std::string ImGuiShaderSourceCode = R"(
#type vertex
#version 450 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;

layout (push_constant) uniform Uniforms
{
    vec2 Scale;
    vec2 Translate;
} u_Uniforms;

struct VertexOutput
{
    vec4 Color;
    vec2 TexCoord;
};

layout(location = 0) out VertexOutput Output;

void main()
{
    gl_Position.xy = a_Position.xy * u_Uniforms.Scale + u_Uniforms.Translate;
    gl_Position.y = -gl_Position.y;
    gl_Position.zw = vec2(0, 1);

    Output.Color = a_Color;
    Output.TexCoord = a_TexCoord;
}

#type fragment
#version 450 core

struct VertexOutput
{
    vec4 Color;
    vec2 TexCoord;
};

layout(location = 0) in VertexOutput Input;

layout(location = 0) out vec4 o_Color;

layout(binding = 0) uniform texture2D u_Texture;
layout(binding = 1) uniform sampler u_TextureSampler;

void main()
{
    o_Color = texture(sampler2D(u_Texture, u_TextureSampler), Input.TexCoord) * Input.Color;
}
	)";

	void ImGuiRenderer::Initialize()
	{
		nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();

		ImGuiIO& io = ImGui::GetIO();

		io.BackendRendererName = "DingoImGuiRenderer";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
		io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)

		m_Shader = ShaderBuilder()
			.SetName("ImGuiRenderer")
			.SetSourceCode(ImGuiShaderSourceCode)
			.SetReflect(false)
			.Create();

		// create attribute layout object
		nvrhi::VertexAttributeDesc vertexAttribLayout[] = {
			{ "POSITION", nvrhi::Format::RG32_FLOAT,  1, 0, offsetof(ImDrawVert,pos), sizeof(ImDrawVert), false },
			{ "TEXCOORD", nvrhi::Format::RG32_FLOAT,  1, 0, offsetof(ImDrawVert,uv),  sizeof(ImDrawVert), false },
			{ "COLOR",    nvrhi::Format::RGBA8_UNORM, 1, 0, offsetof(ImDrawVert,col), sizeof(ImDrawVert), false },
		};

		m_ShaderAttribLayout = device->createInputLayout(vertexAttribLayout, sizeof(vertexAttribLayout) / sizeof(vertexAttribLayout[0]), static_cast<NvrhiShader*>(m_Shader)->m_ShaderHandles[ShaderType::Vertex]);

		// create PSO
		nvrhi::BlendState blendState;
		blendState.targets[0].setBlendEnable(true)
			.setSrcBlend(nvrhi::BlendFactor::SrcAlpha)
			.setDestBlend(nvrhi::BlendFactor::InvSrcAlpha)
			.setSrcBlendAlpha(nvrhi::BlendFactor::One)
			.setDestBlendAlpha(nvrhi::BlendFactor::InvSrcAlpha);

		auto rasterState = nvrhi::RasterState()
			.setFillSolid()
			.setCullNone()
			.setScissorEnable(true)
			.setDepthClipEnable(true);

		auto depthStencilState = nvrhi::DepthStencilState()
			.disableDepthTest()
			.enableDepthWrite()
			.disableStencil()
			.setDepthFunc(nvrhi::ComparisonFunc::Always);

		nvrhi::RenderState renderState;
		renderState.blendState = blendState;
		renderState.depthStencilState = depthStencilState;
		renderState.rasterState = rasterState;

		nvrhi::BindingLayoutDesc layoutDesc;
		layoutDesc.visibility = nvrhi::ShaderType::All;
		layoutDesc.bindingOffsets = { 0, 0, 0 };
		layoutDesc.bindings = {
			nvrhi::BindingLayoutItem::PushConstants(0, sizeof(glm::vec2) * 2),
			nvrhi::BindingLayoutItem::Texture_SRV(0),
			nvrhi::BindingLayoutItem::Sampler(1)
		};
		m_BindingLayout = device->createBindingLayout(layoutDesc);

		m_BasePSODesc.primType = nvrhi::PrimitiveType::TriangleList;
		m_BasePSODesc.inputLayout = m_ShaderAttribLayout;
		m_BasePSODesc.VS = static_cast<NvrhiShader*>(m_Shader)->m_ShaderHandles[ShaderType::Vertex];
		m_BasePSODesc.PS = static_cast<NvrhiShader*>(m_Shader)->m_ShaderHandles[ShaderType::Fragment];
		m_BasePSODesc.renderState = renderState;
		m_BasePSODesc.bindingLayouts = { m_BindingLayout };

		const auto desc = nvrhi::SamplerDesc()
			.setAllAddressModes(nvrhi::SamplerAddressMode::Wrap)
			.setAllFilters(true);

		m_FontSampler = device->createSampler(desc);

		if (m_FontSampler == nullptr)
		{
			return;
		}
	}

	void ImGuiRenderer::Shutdown()
	{
		if (m_FontTexture)
		{
			m_FontTexture->Release();
		}

		if (m_FontSampler)
		{
			m_FontSampler->Release();
		}

		if (m_VertexBuffer)
		{
			m_VertexBuffer->Release();
		}

		if (m_IndexBuffer)
		{
			m_IndexBuffer->Release();
		}

		m_BindingsCache.clear();

		if (m_BindingLayout)
		{
			m_BindingLayout->Release();
		}

		if (m_ShaderAttribLayout)
		{
			m_ShaderAttribLayout->Release();
		}

		m_PipelineCache.clear();

		m_Shader->Destroy();
	}

	void ImGuiRenderer::UpdateFontTexture()
	{
		nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();

		ImGuiIO& io = ImGui::GetIO();

		// If the font texture exists and is bound to ImGui, we're done.
		// Note: ImGui_Renderer will reset io.Fonts->TexID when new fonts are added.
		if (m_FontTexture && io.Fonts->TexID.GetTexID())
		{
			return;
		}

		unsigned char* pixels;
		int width, height;

		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		if (!pixels)
		{
			return;
		}

		nvrhi::TextureDesc textureDesc;
		textureDesc.width = width;
		textureDesc.height = height;
		textureDesc.format = nvrhi::Format::RGBA8_UNORM;
		textureDesc.debugName = "ImGui font texture";
		m_FontTexture = device->createTexture(textureDesc);
		if (m_FontTexture == nullptr)
		{
			return;
		}

		Utils::UploadTextureData(device, m_FontTexture, pixels, width * 4);

		io.Fonts->TexID = (ImTextureID)m_FontTexture.Get();
	}

	bool ImGuiRenderer::Render(ImGuiViewport* viewport, nvrhi::GraphicsPipelineHandle pipeline, nvrhi::FramebufferHandle framebuffer)
	{
		nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();

		ImDrawData* drawData = viewport->DrawData;

		nvrhi::CommandListHandle commandList = device->createCommandList();

		commandList->open();

		// std::string markerName = std::format("ImGui (Viewport {})", viewport == ImGui::GetMainViewport() ? "Main" : std::to_string((uint64_t)viewport));
		// m_CommandList->beginMarker(markerName.c_str());
		//nvrhi::utils::ClearColorAttachment(commandList, framebuffer, 0, nvrhi::Color(1, 0, 1, 1));

		if (!UpdateGeometry(drawData))
		{
			commandList->close();
			return false;
		}

		// handle DPI scaling
		drawData->ScaleClipRects(drawData->FramebufferScale);

		struct PushConstants
		{
			glm::vec2 Scale;
			glm::vec2 Translate;
		} pushConstants;

		pushConstants.Scale.x = 2.0f / drawData->DisplaySize.x;
		pushConstants.Scale.y = 2.0f / drawData->DisplaySize.y;
		pushConstants.Translate.x = -1.0f - drawData->DisplayPos.x * pushConstants.Scale.x;
		pushConstants.Translate.y = -1.0f - drawData->DisplayPos.y * pushConstants.Scale.y;

		float fbWidth = drawData->DisplaySize.x * drawData->FramebufferScale.x;
		float fbHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;

		// set up graphics state
		nvrhi::GraphicsState drawState;

		drawState.framebuffer = framebuffer;
		DE_CORE_ASSERT(drawState.framebuffer);

		drawState.pipeline = pipeline;

		drawState.viewport.viewports.push_back(nvrhi::Viewport(fbWidth, fbHeight));
		drawState.viewport.scissorRects.resize(1);  // updated below

		nvrhi::VertexBufferBinding vbufBinding;
		vbufBinding.buffer = m_VertexBuffer;
		vbufBinding.slot = 0;
		vbufBinding.offset = 0;
		drawState.vertexBuffers.push_back(vbufBinding);

		drawState.indexBuffer.buffer = m_IndexBuffer;
		drawState.indexBuffer.format = (sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT);
		drawState.indexBuffer.offset = 0;

		// Will project scissor/clipping rectangles into framebuffer space
		ImVec2 clip_off = drawData->DisplayPos;         // (0,0) unless using multi-viewports
		ImVec2 clip_scale = drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		// render command lists
		int vtxOffset = 0;
		int idxOffset = 0;
		for (int n = 0; n < drawData->CmdListsCount; n++)
		{
			const ImDrawList* cmdList = drawData->CmdLists[n];
			for (int i = 0; i < cmdList->CmdBuffer.Size; i++)
			{
				const ImDrawCmd* pCmd = &cmdList->CmdBuffer[i];

				if (pCmd->UserCallback)
				{
					pCmd->UserCallback(cmdList, pCmd);
				}
				else
				{
					drawState.bindings = { GetBindingSet((nvrhi::ITexture*)pCmd->GetTexID()) };
					DE_CORE_ASSERT(drawState.bindings[0]);

					// Project scissor/clipping rectangles into framebuffer space
					ImVec2 clipMin((pCmd->ClipRect.x - clip_off.x) * clip_scale.x, (pCmd->ClipRect.y - clip_off.y) * clip_scale.y);
					ImVec2 clipMax((pCmd->ClipRect.z - clip_off.x) * clip_scale.x, (pCmd->ClipRect.w - clip_off.y) * clip_scale.y);

					// Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
					if (clipMin.x < 0.0f)
						clipMin.x = 0.0f;
					if (clipMin.y < 0.0f)
						clipMin.y = 0.0f;
					if (clipMax.x > fbWidth)
						clipMax.x = (float)fbWidth;
					if (clipMax.y > fbHeight)
						clipMax.y = (float)fbHeight;
					if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
						continue;

					drawState.viewport.scissorRects[0] = nvrhi::Rect(clipMin.x, clipMax.x, clipMin.y, clipMax.y);

					nvrhi::DrawArguments drawArguments;
					drawArguments.vertexCount = pCmd->ElemCount;
					drawArguments.startIndexLocation = pCmd->IdxOffset + idxOffset;
					drawArguments.startVertexLocation = pCmd->VtxOffset + vtxOffset;

					commandList->setGraphicsState(drawState);
					commandList->setPushConstants(&pushConstants, sizeof(PushConstants));
					commandList->drawIndexed(drawArguments);
				}

			}

			vtxOffset += cmdList->VtxBuffer.Size;
			idxOffset += cmdList->IdxBuffer.Size;
		}

		commandList->close();
		device->executeCommandList(commandList);

		return true;
	}

	bool ImGuiRenderer::RenderToSwapchain(ImGuiViewport* viewport, SwapChain* swapchain)
	{
		return Render(viewport, GetOrCreatePipeline(swapchain), static_cast<NvrhiFramebuffer*>(swapchain->GetCurrentFramebuffer())->m_FramebufferHandle);
	}

	bool ImGuiRenderer::ReallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer)
	{
		nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();

		if (buffer == nullptr || size_t(buffer->getDesc().byteSize) < requiredSize)
		{
			nvrhi::BufferDesc desc;
			desc.byteSize = uint32_t(reallocateSize);
			desc.structStride = 0;
			desc.debugName = isIndexBuffer ? "ImGui index buffer" : "ImGui vertex buffer";
			desc.canHaveUAVs = false;
			desc.isVertexBuffer = !isIndexBuffer;
			desc.isIndexBuffer = isIndexBuffer;
			desc.isDrawIndirectArgs = false;
			desc.isVolatile = false;
			desc.initialState = isIndexBuffer ? nvrhi::ResourceStates::IndexBuffer : nvrhi::ResourceStates::VertexBuffer;
			desc.keepInitialState = true;

			buffer = device->createBuffer(desc);

			if (!buffer)
			{
				return false;
			}
		}

		return true;
	}

	bool ImGuiRenderer::UpdateGeometry(ImDrawData* drawData)
	{
		nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();

		nvrhi::CommandListHandle commandList = device->createCommandList();

		commandList->open();

		// create/resize vertex and index buffers if needed
		if (!ReallocateBuffer(m_VertexBuffer, drawData->TotalVtxCount * sizeof(ImDrawVert), (drawData->TotalVtxCount + 5000) * sizeof(ImDrawVert), false))
		{
			return false;
		}

		if (!ReallocateBuffer(m_IndexBuffer, drawData->TotalIdxCount * sizeof(ImDrawIdx), (drawData->TotalIdxCount + 5000) * sizeof(ImDrawIdx), true))
		{
			return false;
		}

		m_VertexBufferData.resize(m_VertexBuffer->getDesc().byteSize / sizeof(ImDrawVert));
		m_IndexBufferData.resize(m_IndexBuffer->getDesc().byteSize / sizeof(ImDrawIdx));

		// copy and convert all vertices into a single contiguous buffer
		ImDrawVert* vtxDst = &m_VertexBufferData[0];
		ImDrawIdx* idxDst = &m_IndexBufferData[0];

		for (int n = 0; n < drawData->CmdListsCount; n++)
		{
			const ImDrawList* cmdList = drawData->CmdLists[n];

			memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

			vtxDst += cmdList->VtxBuffer.Size;
			idxDst += cmdList->IdxBuffer.Size;
		}

		commandList->writeBuffer(m_VertexBuffer, &m_VertexBufferData[0], m_VertexBuffer->getDesc().byteSize);
		commandList->writeBuffer(m_IndexBuffer, &m_IndexBufferData[0], m_IndexBuffer->getDesc().byteSize);

		commandList->close();
		device->executeCommandList(commandList);

		return true;
	}

	nvrhi::GraphicsPipelineHandle ImGuiRenderer::GetOrCreatePipeline(SwapChain* swapchain)
	{
		uint32_t currentFramebufferIndex = swapchain->GetCurrentBackBufferIndex();
		auto& swapchainPipelineCache = m_PipelineCache[swapchain];
		DE_CORE_ASSERT(currentFramebufferIndex < swapchainPipelineCache.Pipelines.max_size());

		nvrhi::FramebufferHandle targetFramebuffer = static_cast<NvrhiFramebuffer*>(swapchain->GetCurrentFramebuffer())->m_FramebufferHandle;

		nvrhi::GraphicsPipelineHandle pipeline = swapchainPipelineCache.Pipelines[currentFramebufferIndex];
		bool invalidate = !pipeline || swapchainPipelineCache.Framebuffers[currentFramebufferIndex] != targetFramebuffer;
		if (invalidate)
		{
			nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();
			pipeline = device->createGraphicsPipeline(m_BasePSODesc, targetFramebuffer);
			swapchainPipelineCache.Pipelines[currentFramebufferIndex] = pipeline;
			swapchainPipelineCache.Framebuffers[currentFramebufferIndex] = targetFramebuffer;
		}
		return pipeline;
	}

	nvrhi::IBindingSet* ImGuiRenderer::GetBindingSet(nvrhi::ITexture* texture)
	{
		nvrhi::IDevice* device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();

		auto iter = m_BindingsCache.find(texture);
		if (iter != m_BindingsCache.end())
		{
			return iter->second;
		}

		nvrhi::BindingSetDesc desc;

		desc.bindings = {
			nvrhi::BindingSetItem::PushConstants(0, sizeof(float) * 2),
			nvrhi::BindingSetItem::Texture_SRV(0, texture),
			nvrhi::BindingSetItem::Sampler(1, m_FontSampler)
		};

		nvrhi::BindingSetHandle binding = device->createBindingSet(desc, m_BindingLayout);
		DE_CORE_ASSERT(binding);

		m_BindingsCache[texture] = binding;
		return binding;
	}

} // namespace Dingo
