#pragma once
#include "DingoEngine/Graphics/Shader.h"

namespace Dingo
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

		ShaderBuilder& SetEntryPoint(const std::string& entryPoint)
		{
			m_Params.EntryPoint = entryPoint;
			return *this;
		}

		ShaderBuilder& SetReflect(bool reflect)
		{
			m_Params.Reflect = reflect;
			return *this;
		}

		ShaderBuilder& SetFilePath(const std::filesystem::path& filepath)
		{
			m_Params.FilePath = filepath;
			return *this;
		}

		ShaderBuilder& SetSourceCode(const std::string& source)
		{
			m_Params.SourceCode = source;
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
