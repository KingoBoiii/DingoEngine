#include "depch.h"
#include "NvrhiShader.h"

#include "DingoEngine/Graphics/GraphicsContext.h"

namespace DingoEngine
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

		static std::string GetFileName(const std::filesystem::path& filepath)
		{
			const std::string& filepathString = filepath.string();

			// Extract name from filepath
			auto lastSlash = filepathString.find_last_of("/\\");
			lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
			auto lastDot = filepathString.rfind('.');
			auto count = lastDot == std::string::npos ? filepathString.size() - lastSlash : lastDot - lastSlash;
			return filepathString.substr(lastSlash, count);
		}

	}
	
	void NvrhiShader::Initialize()
	{
		std::string name = m_Params.Name;
		if (name.empty())
		{
			DE_CORE_WARN("Shader name is empty, using file name as shader name.");
		}

		for (const auto& [shaderType, filePath] : m_Params.ShaderFilePaths)
		{
			if (name.empty())
			{
				name = Utils::GetFileName(filePath);
			}

			const std::vector<char> spvBinaries = Utils::ReadSpvFile(filePath.string());
			m_ShaderHandles[shaderType] = CreateShaderHandle(Utils::ConvertShaderTypeToNVRHI(shaderType), spvBinaries, name);
			DE_CORE_INFO("Shader handle created for {} ({}): {}", name, Utils::ConvertShaderTypeToString(shaderType), filePath.string());
		}
	}
	
	void NvrhiShader::Destroy()
	{
		m_ShaderHandles.clear();
	}

	nvrhi::ShaderHandle NvrhiShader::CreateShaderHandle(nvrhi::ShaderType shaderType, const std::vector<char>& spvbinary, const std::string& debugName)
	{
		nvrhi::ShaderDesc shaderDesc = nvrhi::ShaderDesc()
			.setDebugName(debugName)
			.setShaderType(shaderType)
			.setEntryName("main");

		return GraphicsContext::GetDeviceHandle()->createShader(shaderDesc, spvbinary.data(), spvbinary.size());
	}

}
