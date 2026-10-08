#include "depch.h"
#include "DingoEngine/Graphics/FullscreenPass.h"
#include "DingoEngine/Graphics/EngineShaders.h"
#include "DingoEngine/Asset/UnmanagedShaderWatch.h"

namespace Dingo::Internal
{

	FullscreenShader::FullscreenShader(const char* name, std::string_view shaderFile, const std::vector<ShaderDefine>& defines)
	{
		ShaderParams params = ShaderParams().SetName(name);
		for (const ShaderDefine& define : defines)
			params.AddDefine(define.Name, define.Value);

		m_Shader = CreateEngineShader(params, shaderFile);
		WatchUnmanagedShader(m_Shader);
	}

	FullscreenShader::~FullscreenShader()
	{
		UnwatchUnmanagedShader(m_Shader);
		DestroyAndDelete(m_Shader);
	}

	Material* FullscreenShader::CreateMaterial(const std::string& name, BlendMode blend) const
	{
		return Material::Create(MaterialParams()
			.SetDebugName(name)
			.SetShader(m_Shader)
			.SetCullMode(CullMode::None)
			.SetDepthTest(false)
			.SetDepthWrite(false)
			.SetBlendMode(blend));
	}

	void DrawFullscreen(Material* material, Framebuffer* target)
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);
		Renderer::Draw(material, 3);
		Renderer::SetRenderTarget(previous);
	}

	void DrawFullscreen(Material* material, Framebuffer* target, const Viewport& viewport)
	{
		Framebuffer* previous = Renderer::GetRenderTarget();
		Renderer::SetRenderTarget(target);
		Renderer::SetViewport(viewport);
		Renderer::Draw(material, 3);
		Renderer::SetRenderTarget(previous);
	}

}
