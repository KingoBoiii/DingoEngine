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
		virtual ~Shader() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;

		// True when Initialize() actually produced a usable program - Create() never
		// returns nullptr, so this is how a failed load is detected (mirrors Font).
		virtual bool IsValid() const = 0;

		// Recompiles a file-backed shader from its source on disk, bypassing the
		// bytecode disk cache (and rewriting it). On success the generation is bumped
		// and pipelines built from this shader lazily rebuild on their next bind. On
		// compile failure the previous program keeps running and this returns false.
		// Inline-source shaders cannot reload (returns false).
		virtual bool Reload() = 0;

		// Incremented on every successful Reload(). Anything that baked this shader's
		// bytecode or binding layout must rebuild when its recorded generation differs.
		uint32_t GetGeneration() const { return m_Generation; }

		const ShaderParams& GetParams() const { return m_Params; }

	protected:
		ShaderParams m_Params;
		uint32_t m_Generation = 0;

		friend class NvrhiPipeline;
	};

}

