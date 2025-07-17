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
		bool Reflect = true; // Whether to reflect shader resources
		std::filesystem::path FilePath;
		std::string SourceCode; // Optional source code for inline shaders

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

		ShaderParams& SetReflect(bool reflect)
		{
			Reflect = reflect;
			return *this;
		}

		ShaderParams& SetFilePath(const std::filesystem::path& filepath)
		{
			FilePath = filepath;
			return *this;
		}

		ShaderParams& SetSourceCode(const std::string& source)
		{
			SourceCode = source;
			return *this;
		}
	};

	class Shader
	{
	public:
		static Shader* CreateFromFile(const std::string& name, const std::filesystem::path& filepath, bool reflect = true);
		static Shader* CreateFromSource(const std::string& name, const std::string& source, bool reflect = true);
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

