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
		std::string name = m_Params.Name.empty() ? m_Params.FilePath.filename().string() : m_Params.Name;
		if (name.empty())
		{
			DE_CORE_WARN("Shader name is empty, using file name as shader name.");
		}

		std::unordered_map<ShaderType, std::string> sources = GetShaderSources();
		if (sources.empty())
		{
			DE_CORE_ERROR("No shader sources found. Cannot initialize shader.");
			return; // No sources to compile
		}

		ShaderCompiler shaderCompiler;

		std::unordered_map<ShaderType, std::vector<uint32_t>> spvBinaries = CompileOrGetShaderBinaries(sources, name, shaderCompiler);
		if (spvBinaries.empty())
		{
			DE_CORE_ERROR("No shader binaries found. Cannot initialize shader.");
			return; // No binaries to create shader handles
		}

		// Create shader handles for each shader type
		std::vector<ShaderReflection> reflections;
		for (const auto& [shaderType, binaries] : spvBinaries)
		{
			m_ShaderHandles[shaderType] = CreateShaderHandle(Utils::ConvertShaderTypeToNVRHI(shaderType), binaries, name);

			DE_CORE_INFO("Shader handle created for {} ({})", name, Utils::ConvertShaderTypeToString(shaderType));

			if (m_Params.Reflect)
			{
				// Reflect shader resources if needed
				const ShaderReflection& reflection = shaderCompiler.Reflect(shaderType, binaries);
				shaderCompiler.PrintReflection(shaderType, reflection);

				reflections.push_back(reflection);
			}
		}

		CreateBindingLayoutHandle(reflections);
	}

	void NvrhiShader::Destroy()
	{
		m_ShaderHandles.clear();

		if (m_BindingLayoutHandle)
		{
			m_BindingLayoutHandle->Release();
		}
	}

	nvrhi::ShaderHandle NvrhiShader::CreateShaderHandle(nvrhi::ShaderType shaderType, const std::vector<uint32_t>& spvbinary, const std::string& debugName)
	{
		nvrhi::ShaderDesc shaderDesc = nvrhi::ShaderDesc()
			.setDebugName(debugName)
			.setShaderType(shaderType)
			.setEntryName(m_Params.EntryPoint);

		return GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createShader(shaderDesc, spvbinary.data(), spvbinary.size() * 4);
	}

	void NvrhiShader::CreateBindingLayoutHandle(const std::vector<ShaderReflection>& reflections)
	{
		if (!m_Params.Reflect)
		{
			return;
		}

		if (reflections.empty())
		{
			DE_CORE_WARN("No resources found for shader");
			return; // No resources to create binding set
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
				bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::PushConstants(pushConstantBuffer.Binding, 0)); // TODO: Handle size and offset for push constants
			}

			for (const auto& sampler : shaderReflection.SeperateSamplers)
			{
				bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(sampler.Binding)
					.setSize(sampler.ArraySize));
			}

			for (const auto& sampledImage : shaderReflection.SampledImages)
			{
				bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(sampledImage.Binding)
					.setSize(sampledImage.ArraySize));
			}

			for (const auto& sampledImage : shaderReflection.SeperateImages)
			{
				bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(sampledImage.Binding)
					.setSize(sampledImage.ArraySize));
			}
		}

		m_BindingLayoutHandle = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createBindingLayout(bindingLayoutDesc);
	}

	std::unordered_map<ShaderType, std::vector<uint32_t>> NvrhiShader::CompileOrGetShaderBinaries(const std::unordered_map<ShaderType, std::string>& sources, const std::string& name, ShaderCompiler& compiler)
	{
		std::unordered_map<ShaderType, std::vector<uint32_t>> result;

		for (const auto& [shaderType, source] : sources)
		{
			const std::filesystem::path& cachePath = CacheManager::GetCacheDirectory("shaders");
			std::filesystem::path shaderCacheFilePath = cachePath / (name + "_" + Utils::ConvertShaderTypeToString(shaderType) + ".spv");

			if (std::filesystem::exists(shaderCacheFilePath))
			{
				std::ifstream in(shaderCacheFilePath, std::ios::in | std::ios::binary);
				DE_CORE_ASSERT(in.is_open(), "Failed to open shader binary cache file: " + shaderCacheFilePath.string());

				in.seekg(0, std::ios::end);
				auto size = in.tellg();
				in.seekg(0, std::ios::beg);

				auto& data = result[shaderType];
				data.resize(size / sizeof(uint32_t));
				in.read((char*)data.data(), size);

				continue;
			}

			result[shaderType] = compiler.CompileGLSL(shaderType, source, name);

			std::ofstream out(shaderCacheFilePath, std::ios::out | std::ios::binary);
			DE_CORE_ASSERT(out.is_open(), "Failed to create shader binary cache file: " + shaderCacheFilePath.string());

			auto& data = result[shaderType];
			out.write((char*)data.data(), data.size() * sizeof(uint32_t));
			out.flush();
			out.close();
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

		DE_CORE_ASSERT(!m_Params.FilePath.empty(), "Shader file path is empty. Cannot read shader source.");
		DE_CORE_ASSERT(std::filesystem::exists(m_Params.FilePath), "Shader file does not exist.");

		std::string source = FileSystem::ReadTextFile(m_Params.FilePath);

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
