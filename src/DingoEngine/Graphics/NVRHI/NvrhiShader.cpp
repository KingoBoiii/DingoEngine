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
		};

		static constexpr uint32_t k_ShaderCacheMagic = 0x43485344; // "DSHC"
		// Bump when compiler options or the shader toolchain change in a way that
		// invalidates previously cached bytecode.
		static constexpr uint32_t k_ShaderCacheFormatVersion = 1;

		static uint64_t HashFNV1a(std::string_view data, uint64_t hash = 14695981039346656037ull)
		{
			for (const unsigned char c : data)
			{
				hash ^= c;
				hash *= 1099511628211ull;
			}
			return hash;
		}

		static uint64_t ComputeShaderCacheHash(const std::string& source, const std::string& entryPoint, uint32_t shaderModel = 0)
		{
			uint64_t hash = HashFNV1a(source);
			hash = HashFNV1a(entryPoint, hash);
			hash = HashFNV1a(std::string_view(reinterpret_cast<const char*>(&shaderModel), sizeof(shaderModel)), hash);
			return hash;
		}

		static std::string MakeShaderCacheBlob(uint64_t sourceHash, const void* bytecode, size_t size)
		{
			ShaderCacheHeader header;
			header.Magic = k_ShaderCacheMagic;
			header.FormatVersion = k_ShaderCacheFormatVersion;
			header.SourceHash = sourceHash;

			std::string blob(sizeof(header) + size, '\0');
			std::memcpy(blob.data(), &header, sizeof(header));
			std::memcpy(blob.data() + sizeof(header), bytecode, size);
			return blob;
		}

		// False when missing, unreadable, from an older format, or compiled from
		// different source - the caller recompiles, exactly as if the file were absent.
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

			outBytecode.resize(static_cast<size_t>(fileSize) - sizeof(ShaderCacheHeader));
			in.read(reinterpret_cast<char*>(outBytecode.data()), outBytecode.size());
			return static_cast<bool>(in);
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

		std::vector<std::pair<std::filesystem::path, std::string>> pendingCacheWrites;
		std::unordered_map<ShaderType, std::vector<uint32_t>> spvBinaries = CompileOrGetShaderBinaries(sources, name, shaderCompiler, forceCompile, tolerateErrors, pendingCacheWrites);
		if (spvBinaries.empty())
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
		for (const auto& [shaderType, binaries] : spvBinaries)
		{
			nvrhi::ShaderHandle handle;

			if (needsDXBC)
			{
				// D3D12 needs SM 5.1 for NonUniformResourceIndex (nonuniformEXT); D3D11 uses SM 5.0
				const uint32_t shaderModel = (api == GraphicsAPI::DirectX12) ? 51 : 50;

				const std::filesystem::path& cachePath = CacheManager::GetCacheDirectory("shaders");
				std::filesystem::path dxbcCachePath = cachePath / (name + "_" + Utils::ConvertShaderTypeToString(shaderType) + "_sm" + std::to_string(shaderModel) + ".dxbc");

				const uint64_t dxbcHash = Utils::ComputeShaderCacheHash(sources.at(shaderType), m_Params.EntryPoint, shaderModel);

				std::vector<uint8_t> dxbcBytecode;
				if (forceCompile || !Utils::ReadShaderCache(dxbcCachePath, dxbcHash, dxbcBytecode))
				{
					if (!forceCompile && std::filesystem::exists(dxbcCachePath))
						DE_CORE_INFO("DXBC cache for {} ({}) is stale or outdated - recompiling.", name, Utils::ConvertShaderTypeToString(shaderType));

					dxbcBytecode = shaderCompiler.CompileGLSLToHLSLBytecode(shaderType, sources.at(shaderType), name, shaderModel, !tolerateErrors);
					if (dxbcBytecode.empty())
						return false;

					pendingCacheWrites.emplace_back(dxbcCachePath, Utils::MakeShaderCacheBlob(dxbcHash, dxbcBytecode.data(), dxbcBytecode.size()));
				}

				nvrhi::ShaderDesc shaderDesc = nvrhi::ShaderDesc()
					.setDebugName(name)
					.setShaderType(Utils::ConvertShaderTypeToNVRHI(shaderType))
					.setEntryName("main");

				handle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()
					->createShader(shaderDesc, dxbcBytecode.data(), dxbcBytecode.size());
			}
			else
			{
				handle = CreateShaderHandle(Utils::ConvertShaderTypeToNVRHI(shaderType), binaries, name);
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
				const ShaderReflection& reflection = shaderCompiler.Reflect(shaderType, binaries);
				shaderCompiler.PrintReflection(shaderType, reflection);

				reflections.push_back(reflection);
			}
		}

		m_ShaderHandles = std::move(newHandles);
		m_BindingLayoutHandle = CreateBindingLayoutHandle(reflections);

		// Cache files are written only once the WHOLE build succeeded, so a failed
		// stage can't leave mixed old/new bytecode on disk across stages or targets.
		for (const auto& [path, bytes] : pendingCacheWrites)
		{
			std::ofstream out(path, std::ios::out | std::ios::binary);
			DE_CORE_ASSERT(out.is_open(), "Failed to create shader cache file: " + path.string());
			out.write(bytes.data(), bytes.size());
		}

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

	std::unordered_map<ShaderType, std::vector<uint32_t>> NvrhiShader::CompileOrGetShaderBinaries(const std::unordered_map<ShaderType, std::string>& sources, const std::string& name, ShaderCompiler& compiler, bool forceCompile, bool tolerateErrors, std::vector<std::pair<std::filesystem::path, std::string>>& pendingCacheWrites)
	{
		std::unordered_map<ShaderType, std::vector<uint32_t>> result;

		for (const auto& [shaderType, source] : sources)
		{
			const std::filesystem::path& cachePath = CacheManager::GetCacheDirectory("shaders");
			std::filesystem::path shaderCacheFilePath = cachePath / (name + "_" + Utils::ConvertShaderTypeToString(shaderType) + ".spv");

			const uint64_t sourceHash = Utils::ComputeShaderCacheHash(source, m_Params.EntryPoint);

			if (!forceCompile)
			{
				std::vector<uint8_t> cached;
				if (Utils::ReadShaderCache(shaderCacheFilePath, sourceHash, cached))
				{
					auto& data = result[shaderType];
					data.resize(cached.size() / sizeof(uint32_t));
					std::memcpy(data.data(), cached.data(), data.size() * sizeof(uint32_t));
					continue;
				}

				if (std::filesystem::exists(shaderCacheFilePath))
					DE_CORE_INFO("Shader cache for {} ({}) is stale or outdated - recompiling.", name, Utils::ConvertShaderTypeToString(shaderType));
			}

			std::vector<uint32_t> binaries = compiler.CompileGLSL(shaderType, source, name, "main", true, !tolerateErrors);
			if (binaries.empty())
				return {}; // abort the whole build - a partial result must not be committed or cached

			pendingCacheWrites.emplace_back(shaderCacheFilePath, Utils::MakeShaderCacheBlob(sourceHash, binaries.data(), binaries.size() * sizeof(uint32_t)));
			result[shaderType] = std::move(binaries);
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
