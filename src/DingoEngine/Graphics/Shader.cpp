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

	namespace
	{
		int32_t FindBinding(const std::vector<std::pair<std::string, uint32_t>>& bindings, std::string_view wanted)
		{
			for (const auto& [name, binding] : bindings)
			{
				if (name == wanted)
					return static_cast<int32_t>(binding);
			}
			return -1;
		}
	}

	int32_t Shader::FindUniformBufferBinding(std::string_view blockName) const
	{
		return FindBinding(m_UniformBufferBindings, blockName);
	}

	int32_t Shader::FindTextureBinding(std::string_view textureName) const
	{
		return FindBinding(m_TextureBindings, textureName);
	}

	int32_t Shader::FindSamplerBinding(std::string_view samplerName) const
	{
		return FindBinding(m_SamplerBindings, samplerName);
	}

	int32_t Shader::FindStorageBufferBinding(std::string_view blockName) const
	{
		return FindBinding(m_StorageBufferBindings, blockName);
	}

	int32_t Shader::FindStorageImageBinding(std::string_view imageName) const
	{
		return FindBinding(m_StorageImageBindings, imageName);
	}

	bool Shader::IsStorageBufferReadOnly(uint32_t binding) const
	{
		return std::find(m_WritableStorageBuffers.begin(), m_WritableStorageBuffers.end(), binding) == m_WritableStorageBuffers.end();
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
