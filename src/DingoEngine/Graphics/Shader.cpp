#include "depch.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Builders/ShaderBuilder.h"

#include "NVRHI/NvrhiShader.h"

#include "ShaderCompiler.h"

namespace Dingo
{

	Shader* Shader::CreateFromFile(const std::string& name, const std::filesystem::path& filepath, bool reflect)
	{
		return ShaderBuilder()
			.SetName(name)
			.SetFilePath(filepath)
			.SetReflect(reflect)
			.Create();
	}

	Shader* Shader::CreateFromSource(const std::string& name, const std::string& source, bool reflect)
	{
		return ShaderBuilder()
			.SetName(name)
			.SetSourceCode(source)
			.SetReflect(reflect)
			.Create();
	}

	Shader* Shader::Create(const ShaderParams& params)
	{
		return new NvrhiShader(params);
	}

}
