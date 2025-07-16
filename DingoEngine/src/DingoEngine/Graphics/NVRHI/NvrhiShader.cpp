#include "depch.h"
#include "NvrhiShader.h"

#include "DingoEngine/Core/FileSystem.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/ShaderCompiler.h"
#include "NvrhiGraphicsContext.h"

namespace Dingo
{

	namespace Utils
	{

		static std::vector<char> ReadSpvFile(const std::string& filepath)
		{
			std::ifstream file(filepath, std::ios::ate | std::ios::binary);
			DE_CORE_ASSERT(file.is_open(), "Failed to open file: " + filepath);

			size_t fileSize = file.tellg();
			std::vector<char> buffer(fileSize);

			file.seekg(0);
			file.read(buffer.data(), fileSize);

			file.close();

			return buffer;
		}

		static std::string ReadFile(const std::filesystem::path& path)
		{
			std::ifstream file(path, std::ios::ate | std::ios::binary);
			if (!file.is_open())
			{
				throw std::runtime_error("Failed to open file: " + path.string());
			}

			size_t fileSize = file.tellg();
			std::string buffer(fileSize, '\0');
			file.seekg(0);
			file.read(buffer.data(), fileSize);
			file.close();
			return buffer;
		}

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

	void NvrhiShader::Initialize()
	{	
		std::string name = m_Params.Name;
		if (name.empty())
		{
			DE_CORE_WARN("Shader name is empty, using file name as shader name.");
		}

		ShaderCompiler shaderCompiler;

		for(const auto& [shaderType, filePath] : m_Params.ShaderFilePaths)
		{
			if (name.empty())
			{
				name = FileSystem::GetFileName(filePath);
			}

			if (filePath.empty())
			{
				DE_CORE_ERROR("Shader file path is empty for shader type: {}", Utils::ConvertShaderTypeToString(shaderType));
				continue; // Skip if the file path is empty
			}

			if (!std::filesystem::exists(filePath))
			{
				DE_CORE_ERROR("Shader file does not exist: {}", filePath.string());
				continue; // Skip if the file does not exist
			}

			const std::string& source = Utils::ReadFile(filePath);

			const std::vector<uint32_t>& binaries = shaderCompiler.CompileGLSL(shaderType, source, name);

			m_ShaderHandles[shaderType] = CreateShaderHandle(Utils::ConvertShaderTypeToNVRHI(shaderType), binaries, name);

			DE_CORE_INFO("Shader handle created for {} ({}): {}", name, Utils::ConvertShaderTypeToString(shaderType), m_Params.ShaderFilePaths[shaderType].string());

			// Reflect shader resources if needed
			shaderCompiler.Reflect(shaderType, binaries);
		}
	}

	void NvrhiShader::Destroy()
	{
		m_ShaderHandles.clear();
	}

	nvrhi::ShaderHandle NvrhiShader::CreateShaderHandle(nvrhi::ShaderType shaderType, const std::vector<uint32_t>& spvbinary, const std::string& debugName)
	{
		nvrhi::ShaderDesc shaderDesc = nvrhi::ShaderDesc()
			.setDebugName(debugName)
			.setShaderType(shaderType)
			.setEntryName("main");

		return GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle()->createShader(shaderDesc, spvbinary.data(), spvbinary.size() * 4);
	}

}
