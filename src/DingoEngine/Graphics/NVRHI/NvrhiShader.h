#pragma once
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/ShaderCompiler.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiShader : public Shader
	{
	public:
		NvrhiShader(const ShaderParams& params)
			: Shader(params)
		{}
		~NvrhiShader() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

	private:
		nvrhi::ShaderHandle CreateShaderHandle(nvrhi::ShaderType shaderType, const std::vector<uint32_t>& spvbinary, const std::string& debugName = "Shader");
		void CreateBindingLayoutHandle(const std::vector<ShaderReflection>& reflection);

		std::unordered_map<ShaderType, std::vector<uint32_t>> CompileOrGetShaderBinaries(const std::unordered_map<ShaderType, std::string>& sources, const std::string& name, ShaderCompiler& compiler);
		std::unordered_map<ShaderType, std::string> GetShaderSources() const;
		std::unordered_map<ShaderType, std::string> PreProcess(const std::string& source) const;

	private:
		std::unordered_map<ShaderType, nvrhi::ShaderHandle> m_ShaderHandles;
		nvrhi::BindingLayoutHandle m_BindingLayoutHandle;

		friend class NvrhiPipeline;
		friend class NvrhiRenderPass;
		friend class ImGuiRenderer;
	};

}
