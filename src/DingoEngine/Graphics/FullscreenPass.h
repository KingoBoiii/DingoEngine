#pragma once
#include "DingoEngine/Graphics/Renderer.h"

#include <string_view>
#include <vector>

namespace Dingo::Internal
{

	// An engine shader for fullscreen passes: its vertex stage is <DingoEngine/Fullscreen.glsl>. It
	// hot-reloads in Debug. Materials made from it draw one opaque, depth-less triangle; their uniforms
	// bind at 0 and texture/sampler slot i at 1 + 2i / 2 + 2i, as for any material without a scene
	// buffer. Give each set of inputs its own material: changing a material's texture rebuilds its
	// pipelines.
	class FullscreenShader
	{
	public:
		FullscreenShader(const char* name, std::string_view shaderFile, const std::vector<ShaderDefine>& defines = {});
		~FullscreenShader();
		FullscreenShader(const FullscreenShader&) = delete;
		FullscreenShader& operator=(const FullscreenShader&) = delete;

		Shader* GetShader() const { return m_Shader; }
		Material* CreateMaterial(const std::string& name, BlendMode blend = BlendMode::Opaque) const;

	private:
		Shader* m_Shader = nullptr;
	};

	// Makes target current until it goes out of scope, then puts back the render target and the
	// viewport that were current, which SetRenderTarget alone would drop.
	class RenderTargetScope
	{
	public:
		explicit RenderTargetScope(Framebuffer* target)
			: m_Previous(Renderer::GetRenderTarget()), m_Viewport(Renderer::GetViewport())
		{
			Renderer::SetRenderTarget(target);
		}
		~RenderTargetScope()
		{
			Renderer::SetRenderTarget(m_Previous);
			if (m_Viewport)
				Renderer::SetViewport(*m_Viewport);
		}
		RenderTargetScope(const RenderTargetScope&) = delete;
		RenderTargetScope& operator=(const RenderTargetScope&) = delete;

	private:
		Framebuffer* m_Previous;
		std::optional<Viewport> m_Viewport;
	};

	// Into target, current only for the draw: the caller's render target and viewport stay current.
	void DrawFullscreen(Material* material, Framebuffer* target);
	void DrawFullscreen(Material* material, Framebuffer* target, const Viewport& viewport);

}
