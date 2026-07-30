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

	struct ShaderPushConstantBinding : public ShaderResourceBinding
	{
		uint32_t Size;
	};

	struct ShaderImageBinding : public ShaderResourceBinding
	{
		uint32_t Dimension;
		uint32_t ArraySize;
	};

	struct ShaderReflection
	{
		std::vector<ShaderResourceBinding> UniformBuffers;
		std::vector<ShaderResourceBinding> StorageBuffers;
		std::vector<ShaderPushConstantBinding> PushConstantBuffers;
		std::vector<ShaderImageBinding> SeparateSamplers;
		std::vector<ShaderImageBinding> SampledImages;
		std::vector<ShaderImageBinding> SeparateImages;
	};

	class ShaderCompiler
	{
	public:
		ShaderCompiler() = default;
		~ShaderCompiler() = default;

	public:
		// Returns an empty vector on failure. assertOnFailure = false turns a compile
		// error into a plain error log + empty result (hot-reload keeps the app alive
		// through shader typos); the default asserts like initial loads always have.
		std::vector<uint32_t> CompileGLSL(ShaderType shaderType, const std::string& source, const std::string& name, const std::string& entryPoint = "main", bool optimize = true, bool assertOnFailure = true);

		// Compiles GLSL → SPIR-V → HLSL → DXBC for use with DirectX 11 or 12 backends.
		// shaderModel: 50 = SM 5.0 (DX11), 51 = SM 5.1 (DX12, required for NonUniformResourceIndex)
		// Note: SM 6.0+ requires DXC, not D3DCompile — do not pass values >= 60.
		// Returns an empty vector on failure; assertOnFailure as for CompileGLSL.
		std::vector<uint8_t> CompileGLSLToHLSLBytecode(ShaderType shaderType, const std::string& source, const std::string& name, uint32_t shaderModel = 50, bool assertOnFailure = true);

		const ShaderReflection Reflect(ShaderType shaderType, const std::vector<uint32_t> binaries);

		void PrintReflection(ShaderType shader, const ShaderReflection& reflection) const;
	};

} // namespace Dingo


