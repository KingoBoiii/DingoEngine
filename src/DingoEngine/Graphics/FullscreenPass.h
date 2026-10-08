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

	// Into target, current only for the draw: the caller's render target stays current.
	void DrawFullscreen(Material* material, Framebuffer* target);
	void DrawFullscreen(Material* material, Framebuffer* target, const Viewport& viewport);

}
