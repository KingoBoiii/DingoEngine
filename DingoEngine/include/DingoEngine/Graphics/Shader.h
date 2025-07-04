#pragma once
#include "Enums.h"
#include <filesystem>

#include <nvrhi/nvrhi.h>
#include <unordered_map>

namespace DingoEngine
{

	struct ShaderParams
	{
		std::string Name;
		std::unordered_map<ShaderType, std::filesystem::path> ShaderFilePaths;

		ShaderParams& SetName(const std::string& name)
		{
			Name = name;
			return *this;
		}

		ShaderParams& AddShaderType(ShaderType shaderType, const std::filesystem::path& filePath)
		{
			ShaderFilePaths[shaderType] = filePath;
			return *this;
		}
	};

	class Shader
	{
	public:
		static Shader* Create(const std::string& vertexFilePath, const std::string& fragmentFilePath);
		static Shader* Create(const ShaderParams& params);

	public:
		Shader(const ShaderParams& params);
		~Shader() = default;

	public:
		void Initialize();
		void Destroy();

	private:
		nvrhi::ShaderHandle CreateShaderHandle(nvrhi::ShaderType shaderType, const std::vector<char>& spvbinary, const std::string& debugName = "Shader");

	private:
		ShaderParams m_Params;
		std::unordered_map<ShaderType, nvrhi::ShaderHandle> m_ShaderHandles;

		friend class NvrhiPipeline;
	};

}

