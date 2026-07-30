#include "depch.h"
#include "NvrhiShader.h"

#include "DingoEngine/Core/CacheManager.h"
#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/ShaderCompiler.h"
#include "NvrhiGraphicsContext.h"

namespace Dingo
{

	namespace Utils
	{

		static nvrhi::ShaderType ConvertShaderTypeToNVRHI(ShaderType shaderType)
		{
			switch (shaderType)
			{
				case ShaderType::Vertex: return nvrhi::ShaderType::Vertex;
				case ShaderType::Fragment: return nvrhi::ShaderType::Pixel;
				default: break;
			}

			DE_CORE_ASSERT(false, "Unsupported shader type.");
			return nvrhi::ShaderType::All; // Should never reach here
		}

		static std::string ConvertShaderTypeToString(ShaderType shaderType)
		{
			switch (shaderType)
			{
				case ShaderType::Vertex: return "Vertex";
				case ShaderType::Fragment: return "Fragment";
				case ShaderType::Geometry: return "Geometry";
				case ShaderType::Compute: return "Compute";
				case ShaderType::RayGeneration: return "Ray Generation";
				case ShaderType::RayAnyHit: return "Ray Any Hit";
				case ShaderType::RayClosestHit: return "Ray Closest Hit";
				case ShaderType::RayMiss: return "Ray Miss";
				case ShaderType::RayIntersection: return "Ray Intersection";
				case ShaderType::RayCallable: return "Ray Callable";
				default: return "Unknown";
			}
		}

		// Bytecode cache files start with this header, binding the cached bytecode to
		// the exact source it was compiled from - stale caches (edited inline shaders,
		// offline file edits, toolchain changes) are detected and recompiled instead of
		// silently served. Header-less files from older engine versions fail the magic
		// check and recompile the same way.
		struct ShaderCacheHeader
		{
			uint32_t Magic = 0;
			uint32_t FormatVersion = 0;
			uint64_t SourceHash = 0;
			// Size and content hash of the bytecode that follows. None of the fields above
			// change when a file is truncated, so without these a short file with an intact
			// header validates cleanly and feeds partial bytecode to the driver.
			uint64_t PayloadSize = 0;
			uint64_t PayloadHash = 0;
		};

		static constexpr uint32_t k_ShaderCacheMagic = 0x43485344; // "DSHC"
		// Bump when compiler options or the shader toolchain change in a way that
		// invalidates previously cached bytecode.
		static constexpr uint32_t k_ShaderCacheFormatVersion = 2;

		static uint64_t HashFNV1a(std::string_view data, uint64_t hash = 14695981039346656037ull)
		{
			for (const unsigned char c : data)
			{
				hash ^= c;
				hash *= 1099511628211ull;
			}
			return hash;
		}

		// Base hash shared by every cached artifact compiled from the same (source, entryPoint)
		// pair; DeriveShaderCacheHash extends it per target so the source text - the expensive
		// part to hash - is hashed once even though SPIR-V and DXBC each need their own key.
		static uint64_t ComputeShaderSourceHash(const std::string& source, const std::string& entryPoint)
		{
			return HashFNV1a(entryPoint, HashFNV1a(source));
		}

		static uint64_t DeriveShaderCacheHash(uint64_t sourceHash, uint32_t shaderModel)
		{
			return HashFNV1a(std::string_view(reinterpret_cast<const char*>(&shaderModel), sizeof(shaderModel)), sourceHash);
		}

		static std::string MakeShaderCacheBlob(uint64_t sourceHash, const void* bytecode, size_t size)
		{
			ShaderCacheHeader header;
			header.Magic = k_ShaderCacheMagic;
			header.FormatVersion = k_ShaderCacheFormatVersion;
			header.SourceHash = sourceHash;
			header.PayloadSize = size;
			header.PayloadHash = HashFNV1a(std::string_view(static_cast<const char*>(bytecode), size));

			std::string blob(sizeof(header) + size, '\0');
			std::memcpy(blob.data(), &header, sizeof(header));
			std::memcpy(blob.data() + sizeof(header), bytecode, size);
			return blob;
		}

		// False when missing, unreadable, from an older format, compiled from different
		// source, or damaged - the caller recompiles, exactly as if the file were absent.
		static bool ReadShaderCache(const std::filesystem::path& path, uint64_t expectedHash, std::vector<uint8_t>& outBytecode)
		{
			std::error_code ec;
			const uintmax_t fileSize = std::filesystem::file_size(path, ec);
			if (ec || fileSize < sizeof(ShaderCacheHeader))
				return false;

			std::ifstream in(path, std::ios::in | std::ios::binary);
			if (!in.is_open())
				return false;

			ShaderCacheHeader header;
			in.read(reinterpret_cast<char*>(&header), sizeof(header));
			if (!in || header.Magic != k_ShaderCacheMagic || header.FormatVersion != k_ShaderCacheFormatVersion || header.SourceHash != expectedHash)
				return false;

			// Size the payload from the header, not the file, and demand they agree: sizing
			// from the file is what let a truncated cache through.
			if (fileSize - sizeof(ShaderCacheHeader) != header.PayloadSize)
				return false;

			outBytecode.resize(static_cast<size_t>(header.PayloadSize));
			in.read(reinterpret_cast<char*>(outBytecode.data()), outBytecode.size());
			if (!in)
				return false;

			const uint64_t payloadHash = HashFNV1a(std::string_view(reinterpret_cast<const char*>(outBytecode.data()), outBytecode.size()));
			return payloadHash == header.PayloadHash;
		}

		// Writes via a sibling temp file and renames it into place, so a crash or a full
		// disk mid-write leaves either the previous cache or nothing - never a half-written
		// file where the next launch would read it. A failure here only costs a recompile,
		// so it warns rather than asserting (which is compiled out in Release anyway).
		static void WriteShaderCache(const std::filesystem::path& path, const std::string& blob)
		{
			std::filesystem::path tempPath = path;
			tempPath += ".tmp";

			{
				std::ofstream out(tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
				if (!out.is_open())
				{
					DE_CORE_WARN("Could not open shader cache file '{}' for writing - the shader will recompile next launch.", tempPath.string());
					return;
				}

				out.write(blob.data(), blob.size());
				out.close();

				if (!out)
				{
					DE_CORE_WARN("Could not write shader cache file '{}' - the shader will recompile next launch.", tempPath.string());
					std::error_code removeEc;
					std::filesystem::remove(tempPath, removeEc);
					return;
				}
			}

			std::error_code ec;
			std::filesystem::rename(tempPath, path, ec);
			if (ec)
			{
				DE_CORE_WARN("Could not replace shader cache file '{}': {}", path.string(), ec.message());
				std::filesystem::remove(tempPath, ec);
			}
		}

		// Shared by the SPIR-V and DXBC cache artifacts: serves the cached bytes when the hash
		// matches, otherwise compiles and queues the result for a deferred disk write - so a
		// later stage failing the build can still discard everything gathered so far.
		template<typename T, typename CompileFn>
		static std::vector<T> LoadOrCompileCached(const std::filesystem::path& cachePath, uint64_t hash, bool forceCompile, const std::string& name, ShaderType shaderType, const char* kind, std::vector<std::pair<std::filesystem::path, std::string>>& pendingCacheWrites, CompileFn&& compile)
		{
			if (!forceCompile)
			{
				std::vector<uint8_t> cached;
				if (ReadShaderCache(cachePath, hash, cached))
				{
					std::vector<T> data(cached.size() / sizeof(T));
					std::memcpy(data.data(), cached.data(), data.size() * sizeof(T));
					return data;
				}

				if (std::filesystem::exists(cachePath))
					DE_CORE_INFO("{} cache for {} ({}) is stale or outdated - recompiling.", kind, name, ConvertShaderTypeToString(shaderType));
			}

			std::vector<T> compiled = compile();
			if (compiled.empty())
				return {};

			pendingCacheWrites.emplace_back(cachePath, MakeShaderCacheBlob(hash, compiled.data(), compiled.size() * sizeof(T)));
			return compiled;
		}

	}

	static std::unordered_map<std::string, ShaderType> ShaderTypeMap = {
		{"vertex", ShaderType::Vertex},
		{"fragment", ShaderType::Fragment},
		{"geometry", ShaderType::Geometry},
		{"compute", ShaderType::Compute},
		{"raygen", ShaderType::RayGeneration},
		{"anyhit", ShaderType::RayAnyHit},
		{"closesthit", ShaderType::RayClosestHit},
		{"miss", ShaderType::RayMiss},
		{"intersection", ShaderType::RayIntersection},
		{"callable", ShaderType::RayCallable},

		// extras
		{"pixel", ShaderType::Fragment}
	};

	void NvrhiShader::Initialize()
	{
		Build(false, false);
	}

	bool NvrhiShader::Reload()
	{
		if (m_Params.FilePath.empty())
		{
			DE_CORE_WARN("Shader '{}' was not created from a file - cannot hot-reload.", m_Params.Name);
			return false;
		}

		if (!Build(true, true))
		{
			DE_CORE_ERROR("Shader '{}' reload failed - keeping the previous program.", m_Params.Name);
			return false;
		}

		m_Generation++;
		DE_CORE_INFO("Shader '{}' reloaded (generation {}).", m_Params.Name, m_Generation);
		return true;
	}

	bool NvrhiShader::Build(bool forceCompile, bool tolerateErrors)
	{
		std::string name = m_Params.Name.empty() ? m_Params.FilePath.filename().string() : m_Params.Name;
		if (name.empty())
		{
			DE_CORE_WARN("Shader name is empty, using file name as shader name.");
		}

		std::unordered_map<ShaderType, std::string> sources = GetShaderSources();
		if (sources.empty())
		{
			DE_CORE_ERROR("No shader sources found. Cannot initialize shader.");
			return false; // No sources to compile
		}

		ShaderCompiler shaderCompiler;
		const std::filesystem::path cacheDir = CacheManager::GetCacheDirectory("shaders");

		std::vector<std::pair<std::filesystem::path, std::string>> pendingCacheWrites;
		std::unordered_map<ShaderType, CompiledStage> spvStages = CompileOrGetShaderBinaries(sources, name, cacheDir, shaderCompiler, forceCompile, tolerateErrors, pendingCacheWrites);
		if (spvStages.empty())
		{
			DE_CORE_ERROR("No shader binaries found. Cannot initialize shader.");
			return false; // No binaries to create shader handles
		}

		// Create shader handles for each shader type
		const GraphicsAPI api = GraphicsContext::Get().GetParams().GraphicsAPI;
		const bool needsDXBC = (api == GraphicsAPI::DirectX11 || api == GraphicsAPI::DirectX12);

		// Build into locals and commit at the end, so a failed (re)build never leaves
		// the shader half-replaced - the previous program keeps running.
		std::unordered_map<ShaderType, nvrhi::ShaderHandle> newHandles;
		std::vector<ShaderReflection> reflections;
		for (const auto& [shaderType, stage] : spvStages)
		{
			nvrhi::ShaderHandle handle;

			if (needsDXBC)
			{
				// D3D12 needs SM 5.1 for NonUniformResourceIndex (nonuniformEXT); D3D11 uses SM 5.0
				const uint32_t shaderModel = (api == GraphicsAPI::DirectX12) ? 51 : 50;

				std::filesystem::path dxbcCachePath = cacheDir / (name + "_" + Utils::ConvertShaderTypeToString(shaderType) + "_sm" + std::to_string(shaderModel) + ".dxbc");
				const uint64_t dxbcHash = Utils::DeriveShaderCacheHash(stage.SourceHash, shaderModel);

				// CompileGLSLToHLSLBytecode recompiles GLSL -> SPIR-V at zero optimization (better
				// HLSL translation quality than stage.Binaries, which was compiled for reflection
				// at the performance level) before cross-compiling to HLSL/DXBC - the two SPIR-V
				// artifacts are not interchangeable, so this cannot reuse stage.Binaries.
				std::vector<uint8_t> dxbcBytecode = Utils::LoadOrCompileCached<uint8_t>(dxbcCachePath, dxbcHash, forceCompile, name, shaderType, "DXBC", pendingCacheWrites,
					[&]() { return shaderCompiler.CompileGLSLToHLSLBytecode(shaderType, sources.at(shaderType), name, shaderModel, !tolerateErrors); });
				if (dxbcBytecode.empty())
					return false;

				nvrhi::ShaderDesc shaderDesc = nvrhi::ShaderDesc()
					.setDebugName(name)
					.setShaderType(Utils::ConvertShaderTypeToNVRHI(shaderType))
					.setEntryName("main");

				handle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()
					->createShader(shaderDesc, dxbcBytecode.data(), dxbcBytecode.size());
			}
			else
			{
				handle = CreateShaderHandle(Utils::ConvertShaderTypeToNVRHI(shaderType), stage.Binaries, name);
			}

			if (!handle)
			{
				DE_CORE_ERROR("Failed to create shader handle for {} ({}).", name, Utils::ConvertShaderTypeToString(shaderType));
				return false;
			}

			newHandles[shaderType] = handle;

			DE_CORE_INFO("Shader handle created for {} ({})", name, Utils::ConvertShaderTypeToString(shaderType));

			if (m_Params.Reflect)
			{
				// Reflect shader resources if needed
				const ShaderReflection& reflection = shaderCompiler.Reflect(shaderType, stage.Binaries);
				shaderCompiler.PrintReflection(shaderType, reflection);

				reflections.push_back(reflection);
			}
		}

		m_ShaderHandles = std::move(newHandles);
		m_BindingLayoutHandle = CreateBindingLayoutHandle(reflections);

		// Cache files are written only once the WHOLE build succeeded, so a failed
		// stage can't leave mixed old/new bytecode on disk across stages or targets.
		for (const auto& [path, bytes] : pendingCacheWrites)
			Utils::WriteShaderCache(path, bytes);

		return true;
	}

	void NvrhiShader::Destroy()
	{
		m_ShaderHandles.clear();
		m_BindingLayoutHandle = nullptr;
	}

	nvrhi::ShaderHandle NvrhiShader::CreateShaderHandle(nvrhi::ShaderType shaderType, const std::vector<uint32_t>& spvbinary, const std::string& debugName)
	{
		nvrhi::ShaderDesc shaderDesc = nvrhi::ShaderDesc()
			.setDebugName(debugName)
			.setShaderType(shaderType)
			.setEntryName(m_Params.EntryPoint);

		return GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createShader(shaderDesc, spvbinary.data(), spvbinary.size() * 4);
	}

	nvrhi::BindingLayoutHandle NvrhiShader::CreateBindingLayoutHandle(const std::vector<ShaderReflection>& reflections)
	{
		if (!m_Params.Reflect)
		{
			return nullptr;
		}

		if (reflections.empty())
		{
			DE_CORE_WARN("No resources found for shader");
			return nullptr; // No resources to create binding set
		}

		nvrhi::VulkanBindingOffsets vulkanBindingOffsets = nvrhi::VulkanBindingOffsets()
			.setSamplerOffset(0)
			.setConstantBufferOffset(0);

		nvrhi::BindingLayoutDesc bindingLayoutDesc = nvrhi::BindingLayoutDesc()
			.setRegisterSpace(0) // set = 0
			.setRegisterSpaceIsDescriptorSet(true)
			.setBindingOffsets(vulkanBindingOffsets)
			.setVisibility(nvrhi::ShaderType::All);

		for (const auto& shaderReflection : reflections)
		{
			for (const auto& uniformBuffer : shaderReflection.UniformBuffers)
			{
				bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(uniformBuffer.Binding));
			}

			for (const auto& storageBuffer : shaderReflection.StorageBuffers)
			{
				bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::RawBuffer_UAV(storageBuffer.Binding));
			}

			for (const auto& pushConstantBuffer : shaderReflection.PushConstantBuffers)
			{
				bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::PushConstants(pushConstantBuffer.Binding, pushConstantBuffer.Size));
			}

			for (const auto& sampler : shaderReflection.SeparateSamplers)
			{
				bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(sampler.Binding)
					.setSize(sampler.ArraySize));
			}

			for (const auto& sampledImage : shaderReflection.SampledImages)
			{
				bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(sampledImage.Binding)
					.setSize(sampledImage.ArraySize));
			}

			for (const auto& sampledImage : shaderReflection.SeparateImages)
			{
				bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(sampledImage.Binding)
					.setSize(sampledImage.ArraySize));
			}
		}

		return GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createBindingLayout(bindingLayoutDesc);
	}

	std::unordered_map<ShaderType, NvrhiShader::CompiledStage> NvrhiShader::CompileOrGetShaderBinaries(const std::unordered_map<ShaderType, std::string>& sources, const std::string& name, const std::filesystem::path& cacheDir, ShaderCompiler& compiler, bool forceCompile, bool tolerateErrors, std::vector<std::pair<std::filesystem::path, std::string>>& pendingCacheWrites)
	{
		std::unordered_map<ShaderType, CompiledStage> result;

		for (const auto& [shaderType, source] : sources)
		{
			std::filesystem::path shaderCacheFilePath = cacheDir / (name + "_" + Utils::ConvertShaderTypeToString(shaderType) + ".spv");
			const uint64_t sourceHash = Utils::ComputeShaderSourceHash(source, m_Params.EntryPoint);
			const uint64_t spvHash = Utils::DeriveShaderCacheHash(sourceHash, 0);

			std::vector<uint32_t> binaries = Utils::LoadOrCompileCached<uint32_t>(shaderCacheFilePath, spvHash, forceCompile, name, shaderType, "Shader", pendingCacheWrites,
				[&]() { return compiler.CompileGLSL(shaderType, source, name, "main", true, !tolerateErrors); });
			if (binaries.empty())
				return {}; // abort the whole build - a partial result must not be committed or cached

			result[shaderType] = CompiledStage{ std::move(binaries), sourceHash };
		}

		return result;
	}

	std::unordered_map<ShaderType, std::string> NvrhiShader::GetShaderSources() const
	{
		if (m_Params.FilePath.empty())
		{
			// If the shader is created from source code, return the preprocessed sources
			return PreProcess(m_Params.SourceCode);
		}

		// Soft failures (build aborts, previous program stays): hot-reload can race an
		// editor save that briefly removes/truncates the file.
		if (!std::filesystem::exists(m_Params.FilePath))
		{
			DE_CORE_ERROR("Shader file does not exist: '{}'.", m_Params.FilePath.string());
			return {};
		}

		std::string source = FileSystem::ReadTextFile(m_Params.FilePath);
		if (source.empty())
		{
			DE_CORE_ERROR("Shader file is empty or unreadable: '{}'.", m_Params.FilePath.string());
			return {};
		}

		return PreProcess(source);
	}

	std::unordered_map<ShaderType, std::string> NvrhiShader::PreProcess(const std::string& source) const
	{
		DE_CORE_ASSERT(!source.empty(), "Shader source code is empty. Cannot preprocess shader sources.");

		std::unordered_map<ShaderType, std::string> sources;

		const char* typeToken = "#type";
		size_t typeTokenLength = strlen(typeToken);
		size_t pos = source.find(typeToken, 0); //Start of shader type declaration line
		while (pos != std::string::npos)
		{
			size_t eol = source.find_first_of("\r\n", pos); //End of shader type declaration line
			DE_CORE_ASSERT(eol != std::string::npos, "Syntax error");
			size_t begin = pos + typeTokenLength + 1; //Start of shader type name (after "#type " keyword)
			std::string type = source.substr(begin, eol - begin);

			if (ShaderTypeMap.find(type) == ShaderTypeMap.end())
			{
				DE_CORE_ERROR("Unknown shader type: {}", type);
				DE_CORE_ASSERT(false, "Unknown shader type");
				return {}; // Return empty map if unknown shader type
			}

			ShaderType shaderType = ShaderTypeMap[type];

			size_t nextLinePos = source.find_first_not_of("\r\n", eol); //Start of shader code after shader type declaration line
			DE_CORE_ASSERT(nextLinePos != std::string::npos, "Syntax error");
			pos = source.find(typeToken, nextLinePos); //Start of next shader type declaration line

			sources[shaderType] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
		}

		return sources;
	}

}
