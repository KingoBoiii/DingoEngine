#pragma once
#include "DingoEngine/Graphics/Shader.h"

namespace DingoEngine
{
	
	class ShaderBuilder
	{
	public:
		ShaderBuilder() = default;
		~ShaderBuilder() = default;

		ShaderBuilder& SetName(const std::string& name)
		{
			m_Params.Name = name;
			return *this;
		}

		ShaderBuilder& AddShaderType(ShaderType shaderType, const std::filesystem::path& filePath)
		{
			m_Params.ShaderFilePaths[shaderType] = filePath;
			return *this;
		}

		Shader* Create() const
		{
			Shader* shader = Shader::Create(m_Params);
			shader->Initialize();
			return shader;
		}

	private:
		ShaderParams m_Params;
	};

}
