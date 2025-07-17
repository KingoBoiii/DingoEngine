#pragma once
#include "DingoEngine/Graphics/Enums/ShaderType.h"

namespace Dingo
{

	struct ShaderResourceBinding
	{
		std::string Name;
		uint32_t DescriptorSet;
		uint32_t Binding;
	};

	struct ShaderReflection
	{
		std::vector<ShaderResourceBinding> UniformBuffers;
		std::vector<ShaderResourceBinding> StorageBuffers;
		std::vector<ShaderResourceBinding> PushConstantBuffers;
		std::vector<ShaderResourceBinding> SeperateSamplers;
		std::vector<ShaderResourceBinding> SampledImages;
		std::vector<ShaderResourceBinding> SeperateImages;
	};

	class ShaderCompiler
	{
	public:
		ShaderCompiler() = default;
		~ShaderCompiler() = default;

	public:
		std::vector<uint32_t> CompileGLSL(ShaderType shaderType, const std::string& source, const std::string& name, const std::string& entryPoint = "main", bool optimize = true);

		const ShaderReflection Reflect(ShaderType shaderType, const std::vector<uint32_t> binaries);

		void PrintReflection(ShaderType shader, const ShaderReflection& reflection) const;
	};

} // namespace Dingo


