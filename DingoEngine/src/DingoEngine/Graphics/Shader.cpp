#include "depch.h"
#include "DingoEngine/Graphics/Shader.h"

#include "NVRHI/NvrhiShader.h"

namespace Dingo
{

	Shader* Shader::Create(const std::string& vertexFilePath, const std::string& fragmentFilePath)
	{
		ShaderParams params = ShaderParams()
			.AddShaderType(ShaderType::Vertex, vertexFilePath)
			.AddShaderType(ShaderType::Fragment, fragmentFilePath);

		return Create(params);
	}

	Shader* Shader::Create(const ShaderParams& params)
	{
		return new NvrhiShader(params);
	}

}
