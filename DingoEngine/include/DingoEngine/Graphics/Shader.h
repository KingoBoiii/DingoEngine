#pragma once

#include <nvrhi/nvrhi.h>

namespace DingoEngine
{

	class Shader
	{
	public:
		static Shader* Create(const std::string& vertexFilePath, const std::string& fragmentFilePath);

	public:
		Shader(const std::string& vertexFilePath, const std::string& fragmentFilePath);
		~Shader() = default;

	public:
		void Initialize();
		void Destroy();

	private:
		nvrhi::ShaderHandle CreateShaderHandle(nvrhi::ShaderType shaderType, const std::vector<char>& spvbinary);

	private:
		std::string m_VertexFilePath;
		std::string m_FragmentFilePath;

		nvrhi::ShaderHandle m_VertexShaderHandle;
		nvrhi::ShaderHandle m_FragmentShaderHandle;

		friend class Pipeline;
	};

}

