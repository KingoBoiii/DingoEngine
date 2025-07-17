#include "depch.h"
#include "DingoEngine/Graphics/Shader.h"

#include "NVRHI/NvrhiShader.h"

#include "ShaderCompiler.h"

namespace Dingo
{

	Shader* Shader::CreateFromFile(const std::string& name, const std::filesystem::path& filepath, bool reflect)
	{
		return Create(ShaderParams()
			.SetName(name)
			.SetFilePath(filepath)
			.SetReflect(reflect)
		);
	}

	Shader* Shader::CreateFromSource(const std::string& name, const std::string& source, bool reflect)
	{
		return Create(ShaderParams()
			.SetName(name)
			.SetSourceCode(source)
			.SetReflect(reflect)
		);
	}

	Shader* Shader::Create(const ShaderParams& params)
	{
		return new NvrhiShader(params);
	}

}
