#pragma once
#include "Enums.h"

#include <filesystem>
#include <unordered_map>

namespace Dingo
{

	struct ShaderParams
	{
		std::string Name;
		std::string EntryPoint = "main"; // Default entry point for shaders
		std::filesystem::path FilePath;
		std::unordered_map<ShaderType, std::filesystem::path> ShaderFilePaths;

		ShaderParams& SetName(const std::string& name)
		{
			Name = name;
			return *this;
		}

		ShaderParams& SetEntryPoint(const std::string& entryPoint)
		{
			EntryPoint = entryPoint;
			return *this;
		}

		ShaderParams& SetFilePath(const std::filesystem::path& filepath)
		{
			FilePath = filepath;
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
		Shader(const ShaderParams& params)
			: m_Params(params)
		{}
		~Shader() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;

	protected:
		ShaderParams m_Params;

		friend class NvrhiPipeline;
	};

}

