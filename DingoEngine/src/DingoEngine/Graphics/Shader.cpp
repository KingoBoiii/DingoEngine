#include "depch.h"
#include "DingoEngine/Graphics/Shader.h"

#include "NVRHI/NvrhiShader.h"

#include "ShaderCompiler.h"

namespace Dingo
{

	Shader* Shader::Create(const ShaderParams& params)
	{
		return new NvrhiShader(params);
	}

}
