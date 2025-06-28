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

	}

	Shader* Shader::Create(const std::string& vertexFilePath, const std::string& fragmentFilePath)
	{
		return new Shader(vertexFilePath, fragmentFilePath);
	}

	Shader::Shader(const std::string& vertexFilePath, const std::string& fragmentFilePath)
		: m_VertexFilePath(vertexFilePath), m_FragmentFilePath(fragmentFilePath)
	{}

	void Shader::Initialize()
	{
		const std::vector<char> vertexSpvBinaries = Utils::ReadSpvFile(m_VertexFilePath);
		m_VertexShaderHandle = CreateShaderHandle(nvrhi::ShaderType::Vertex, vertexSpvBinaries);

		const std::vector<char> fragmentSpvBinaries = Utils::ReadSpvFile(m_FragmentFilePath);
		m_FragmentShaderHandle = CreateShaderHandle(nvrhi::ShaderType::Pixel, fragmentSpvBinaries);
	}

	void Shader::Destroy()
	{
		m_VertexShaderHandle->Release();
		m_FragmentShaderHandle->Release();
	}

	nvrhi::ShaderHandle Shader::CreateShaderHandle(nvrhi::ShaderType shaderType, const std::vector<char>& spvbinary)
	{
		nvrhi::ShaderDesc shaderDesc = nvrhi::ShaderDesc()
			.setShaderType(shaderType)
			.setDebugName("Shader")
			.setEntryName("main");

		return GraphicsContext::GetDeviceHandle()->createShader(shaderDesc, spvbinary.data(), spvbinary.size());
	}

}
