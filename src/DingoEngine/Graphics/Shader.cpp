#include "depch.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Asset/AssetPath.h"
#include "DingoEngine/Core/FileSystem.h"

#include "PushConstants.h"

#include "NVRHI/NvrhiShader.h"

#include "ShaderCompiler.h"

#include <cstring>

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

	uint32_t Shader::FindPushConstantBlockSize(std::string_view blockName) const
	{
		for (const auto& [name, size] : m_PushConstantBlocks)
		{
			if (name == blockName)
				return size;
		}
		return 0;
	}

	uint32_t Shader::GetPushConstantBlockSize() const
	{
		return m_PushConstantBlocks.empty() ? 0 : m_PushConstantBlocks.front().second;
	}

	bool Internal::CopyPushConstants(const Shader* shader, const void* data, uint32_t size, uint8_t* destination, std::string_view ownerKind, std::string_view ownerName)
	{
		const uint32_t blockSize = shader ? shader->GetPushConstantBlockSize() : 0;
		if (blockSize == 0)
		{
			DE_CORE_ERROR("{} '{}': shader '{}' declares no push-constant block.", ownerKind, ownerName, shader ? shader->GetParams().Name : std::string("(none)"));
			return false;
		}
		if (size > blockSize)
		{
			DE_CORE_ERROR("{} '{}': {} bytes of push constants, but shader '{}' has a {}-byte block.", ownerKind, ownerName, size, shader->GetParams().Name, blockSize);
			return false;
		}
		if (size > 0 && !data)
		{
			DE_CORE_ERROR("{} '{}': push constants with {} bytes but no data.", ownerKind, ownerName, size);
			return false;
		}

		std::memset(destination, 0, Shader::k_MaxPushConstantSize);
		if (size > 0)
			std::memcpy(destination, data, size);
		return true;
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
