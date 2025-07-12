#pragma once
#include "DingoEngine/Graphics/Shader.h"

#include <nvrhi/nvrhi.h>

namespace DingoEngine
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
		nvrhi::ShaderHandle CreateShaderHandle(nvrhi::ShaderType shaderType, const std::vector<char>& spvbinary, const std::string& debugName = "Shader");

	private:
		std::unordered_map<ShaderType, nvrhi::ShaderHandle> m_ShaderHandles;

		friend class NvrhiPipeline;
		friend class ImGuiRenderer;
	};

}
