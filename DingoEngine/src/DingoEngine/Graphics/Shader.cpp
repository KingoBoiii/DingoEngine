#include "depch.h"
#include "DingoEngine/Graphics/Shader.h"
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

	Shader* Shader::Create(const std::string& vertexFilePath, const std::string& fragmentFilePath)
	{
		ShaderParams params = ShaderParams()
			.AddShaderType(ShaderType::Vertex, vertexFilePath)
			.AddShaderType(ShaderType::Fragment, fragmentFilePath);

		return new Shader(params);
	}

	Shader* Shader::Create(const ShaderParams& params)
	{
		return new Shader(params);
	}

	Shader::Shader(const ShaderParams& params)
		: m_Params(params)
	{}

	void Shader::Initialize()
	{
		for(const auto& [shaderType, filePath] : m_Params.ShaderFilePaths)
		{
			const std::string& fileName = Utils::GetFileName(filePath);

			const std::vector<char> spvBinaries = Utils::ReadSpvFile(filePath.string());
			m_ShaderHandles[shaderType] = CreateShaderHandle(Utils::ConvertShaderTypeToNVRHI(shaderType), spvBinaries);
			DE_CORE_INFO("Shader handle created for {}: {}", fileName, filePath.string());
		}
	}

	void Shader::Destroy()
	{
		m_ShaderHandles.clear();
	}

	nvrhi::ShaderHandle Shader::CreateShaderHandle(nvrhi::ShaderType shaderType, const std::vector<char>& spvbinary, const std::string& debugName)
	{
		nvrhi::ShaderDesc shaderDesc = nvrhi::ShaderDesc()
			.setShaderType(shaderType)
			.setDebugName(debugName)
			.setEntryName("main");

		return GraphicsContext::GetDeviceHandle()->createShader(shaderDesc, spvbinary.data(), spvbinary.size());
	}

}
