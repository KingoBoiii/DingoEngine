#include "depch.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Core/FileSystem.h"

#include "NVRHI/NvrhiShader.h"

#include "ShaderCompiler.h"

namespace Dingo
{

	Shader* Shader::CreateFromFile(const std::string& name, const std::filesystem::path& filepath, bool reflect)
	{
		return Create(ShaderParams()
			.SetName(name)
			.SetSourceCode(FileSystem::ReadTextFile(filepath))
			.SetReflect(reflect));
	}

	Shader* Shader::CreateFromSource(const std::string& name, const std::string& source, bool reflect)
	{
		return Create(ShaderParams()
			.SetName(name)
			.SetSourceCode(source)
			.SetReflect(reflect));
	}

	Shader* Shader::Create(const ShaderParams& params)
	{
		Shader* shader = new NvrhiShader(params);
		shader->Initialize();
		return shader;
	}

}
