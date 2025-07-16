#pragma once
#include "DingoEngine/Graphics/Enums/ShaderType.h"

namespace Dingo
{

	class ShaderCompiler
	{
	public:
		ShaderCompiler() = default;
		~ShaderCompiler() = default;

	public:
		std::unordered_map<ShaderType, std::vector<uint32_t>> CompileGLSL(std::unordered_map<ShaderType, std::string> sources, const std::string& name, bool optimize = true);
		std::vector<uint32_t> CompileGLSL(ShaderType shaderType, const std::string& source, const std::string& name, bool optimize = true);

		void Reflect(ShaderType shaderType, const std::vector<uint32_t> binaries);
	};

} // namespace Dingo


