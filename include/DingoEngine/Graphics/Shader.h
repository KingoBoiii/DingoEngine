#pragma once
#include "Enums.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Dingo
{

	struct ShaderDefine
	{
		std::string Name;
		std::string Value;
	};

	struct ShaderParams
	{
		std::string Name;
		std::string EntryPoint = "main"; // Default entry point for shaders
		bool Reflect = true; // Whether to reflect shader resources
		std::filesystem::path FilePath;
		std::string SourceCode; // Optional source code for inline shaders
		// Either source may #include another file: "Name.glsl" next to the file (an inline shader looks
		// under the asset root, then the working directory) or <DingoEngine/Name.glsl> for one of the
		// engine's own (Fullscreen.glsl). An included file is part of the bytecode cache key, and a
		// hot-reload watches it too.
		// Preprocessor macros for every stage, e.g. DE_SKINNED. They are part of the bytecode cache
		// key and survive Reload, so one source file can back several variants.
		std::vector<ShaderDefine> Defines;

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

		ShaderParams& AddDefine(const std::string& name, const std::string& value = {})
		{
			Defines.push_back({ name, value });
			return *this;
		}
	};

	class Shader
	{
	public:
		// A relative filepath is looked up under the asset root first, then the working
		// directory.
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

		// The binding of the uniform block with that name in any stage, or -1. Only reflected
		// shaders (ShaderParams::Reflect) know their blocks.
		int32_t FindUniformBufferBinding(std::string_view blockName) const;
		// The same for a separate texture or sampler, by its variable name.
		int32_t FindTextureBinding(std::string_view textureName) const;
		int32_t FindSamplerBinding(std::string_view samplerName) const;
		// The same for a storage buffer block or a storage image.
		int32_t FindStorageBufferBinding(std::string_view blockName) const;
		int32_t FindStorageImageBinding(std::string_view imageName) const;
		// Whether every stage declares the storage buffer at that binding readonly: it binds as a
		// shader-resource view then, which a vertex stage on D3D11 needs; a writable one binds as an
		// unordered-access view, for compute.
		bool IsStorageBufferReadOnly(uint32_t binding) const;

		// Every file the source pulled in with #include on its last build (see ShaderParams), which a
		// hot-reload watches as well as the shader's own file. Engine shaders read from the library,
		// not the disk, aren't listed.
		const std::vector<std::filesystem::path>& GetIncludedFiles() const { return m_IncludedFiles; }

	protected:
		ShaderParams m_Params;
		uint32_t m_Generation = 0;
		std::vector<std::pair<std::string, uint32_t>> m_UniformBufferBindings;
		std::vector<std::pair<std::string, uint32_t>> m_TextureBindings;
		std::vector<std::pair<std::string, uint32_t>> m_SamplerBindings;
		std::vector<std::pair<std::string, uint32_t>> m_StorageBufferBindings;
		std::vector<std::pair<std::string, uint32_t>> m_StorageImageBindings;
		std::vector<uint32_t> m_WritableStorageBuffers; // bindings any stage writes
		std::vector<std::filesystem::path> m_IncludedFiles;

		friend class NvrhiPipeline;
	};

}

