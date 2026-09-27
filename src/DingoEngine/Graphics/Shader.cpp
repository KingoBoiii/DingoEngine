#include "depch.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Asset/AssetPath.h"
#include "DingoEngine/Core/FileSystem.h"

#include "NVRHI/NvrhiShader.h"

#include "ShaderCompiler.h"

namespace Dingo
{

	Shader* Shader::CreateFromFile(const std::string& name, const std::filesystem::path& filepath, bool reflect)
	{
		// Keep the path (instead of eagerly reading the source) so the shader can
		// re-read the file on hot-reload.
		return Create(ShaderParams()
			.SetName(name)
			.SetFilePath(filepath)
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
		ShaderParams resolvedParams = params;
		resolvedParams.FilePath = Internal::ResolveRawAssetPath(params.FilePath);

		Shader* shader = new NvrhiShader(resolvedParams);
		shader->Initialize();
		return shader;
	}

}
