#pragma once
#include "DingoEngine/Graphics/Material.h"

namespace Dingo::Internal
{

	struct MaterialAccess
	{
		static void SetSceneUniformBuffer(Material* material, GraphicsBuffer* buffer) { material->SetSceneUniformBuffer(buffer); }
		static void SetSkinUniformBuffer(Material* material, GraphicsBuffer* buffer) { material->SetSkinUniformBuffer(buffer); }
		static void SetSkinPaletteBuffer(Material* material, GraphicsBuffer* palette) { material->SetSkinPaletteBuffer(palette); }
		static void SetDrawConstants(Material* material, const glm::uvec4& constants) { material->SetDrawConstants(constants); }
		static const uint8_t* GetPushConstantData(const Material* material) { return material->GetPushConstantData(); }
		static void SetShadowResources(Material* material, GraphicsBuffer* shadowData, Texture* atlas, Sampler* sampler) { material->SetShadowResources(shadowData, atlas, sampler); }
		static void SetObjectBuffer(Material* material, GraphicsBuffer* buffer) { material->SetObjectBuffer(buffer); }
		static void SetInstanceBuffers(Material* material, GraphicsBuffer* records, GraphicsBuffer* draw) { material->SetInstanceBuffers(records, draw); }

		static GraphicsBuffer* GetUniformBuffer(const Material* material) { return material->GetUniformBuffer(); }
		static const std::vector<uint8_t>& GetUniformCPUData(const Material* material) { return material->GetUniformCPUData(); }
		static bool NeedsUniformUpload(const Material* material, uint64_t frameIndex) { return material->NeedsUniformUpload(frameIndex); }
		static void MarkUniformUploaded(Material* material, uint64_t frameIndex) { material->MarkUniformUploaded(frameIndex); }

		static RenderPass* GetOrCreateRenderPass(Material* material, const VertexLayout& layout, Framebuffer* framebuffer) { return material->GetOrCreateRenderPass(layout, framebuffer); }
		static RenderPass* GetOrCreateRenderPass(Material* material, const VertexLayout& layout, Framebuffer* framebuffer, CullMode cullMode) { return material->GetOrCreateRenderPass(layout, framebuffer, cullMode); }
	};

}
