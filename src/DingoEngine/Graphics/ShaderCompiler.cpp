#include "depch.h"
#include "ShaderCompiler.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <spirv_cross/spirv_hlsl.hpp>

#ifdef DE_PLATFORM_WINDOWS
#include <d3dcompiler.h>
#endif

namespace Dingo
{

	namespace Utils
	{

#ifdef DE_PLATFORM_WINDOWS
		static std::string GetHLSLTarget(ShaderType shaderType, uint32_t shaderModel)
		{
			std::string smStr = std::to_string(shaderModel / 10) + "_" + std::to_string(shaderModel % 10);
			switch (shaderType)
			{
				case ShaderType::Vertex:   return "vs_" + smStr;
				case ShaderType::Fragment: return "ps_" + smStr;
				case ShaderType::Geometry: return "gs_" + smStr;
				case ShaderType::Compute:  return "cs_" + smStr;
				default:
					DE_CORE_ASSERT(false, "Unsupported shader type for HLSL compilation.");
					return "";
			}
		}
#endif

		static shaderc_shader_kind ConvertShaderTypeToShaderC(ShaderType shaderType)
		{
			switch (shaderType)
			{
				case ShaderType::Vertex: return shaderc_vertex_shader;
				case ShaderType::Fragment: return shaderc_fragment_shader;
				case ShaderType::Geometry: return shaderc_geometry_shader;
				case ShaderType::Compute: return shaderc_compute_shader;
				default:
					DE_CORE_ASSERT(false, "Unsupported shader type.");
					return shaderc_glsl_infer_from_source;
			}
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

	}

	std::vector<uint32_t> ShaderCompiler::CompileGLSL(ShaderType shaderType, const std::string& source, const std::string& name, const std::string& entryPoint, bool optimize, bool assertOnFailure)
	{
		shaderc::Compiler compiler;
		shaderc::CompileOptions compileOptions;
		compileOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
		compileOptions.SetOptimizationLevel(optimize ? shaderc_optimization_level_performance : shaderc_optimization_level_zero);
		compileOptions.SetGenerateDebugInfo();

		shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, Utils::ConvertShaderTypeToShaderC(shaderType), name.c_str(), entryPoint.c_str(), compileOptions);
		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			DE_CORE_ERROR("Shader compilation failed: {}", result.GetErrorMessage());
			if (assertOnFailure)
			{
				DE_CORE_ASSERT(false);
			}
			return {};
		}

		return std::vector<uint32_t>(result.begin(), result.end());
	}

	const ShaderReflection ShaderCompiler::Reflect(ShaderType shaderType, const std::vector<uint32_t> binaries)
	{
		ShaderReflection reflection;

		spirv_cross::Compiler compiler(binaries);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		for (const auto& resource : resources.uniform_buffers)
		{
			const auto& name = resource.name;
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			int memberCount = bufferType.member_types.size();
			uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);

			reflection.UniformBuffers.push_back({ name, descriptorSet, binding });
		}

		for (const auto& resource : resources.storage_buffers)
		{
			const auto& name = resource.name;
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			int memberCount = bufferType.member_types.size();
			uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);

			reflection.StorageBuffers.push_back({ name, descriptorSet, binding });
		}

		for (const auto& resource : resources.push_constant_buffers)
		{
			const auto& name = resource.name;
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);

			reflection.PushConstantBuffers.push_back({ name, descriptorSet, binding, size });
		}

		for (const auto& resource : resources.separate_samplers)
		{
			const auto& name = resource.name;
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);

			uint32_t dimension = 0;
			switch (baseType.image.dim)
			{
				case spv::Dim::Dim1D:
					dimension = 1;
					break;
				case spv::Dim::Dim2D:
					dimension = 2;
					break;
				case spv::Dim::Dim3D:
				case spv::Dim::DimCube:
					dimension = 3;
					break;
			}
			uint32_t arraySize = type.array.size() > 0 ? type.array[0] : 1;
			if (arraySize == 0)
			{
				arraySize = 1;
			}

			reflection.SeparateSamplers.push_back({ name, descriptorSet, binding, dimension, arraySize });
		}

		for (const auto& resource : resources.sampled_images)
		{
			const auto& name = resource.name;
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);

			uint32_t dimension = 0;
			switch (baseType.image.dim)
			{
				case spv::Dim::Dim1D:
					dimension = 1;
					break;
				case spv::Dim::Dim2D:
					dimension = 2;
					break;
				case spv::Dim::Dim3D:
				case spv::Dim::DimCube:
					dimension = 3;
					break;
			}
			uint32_t arraySize = type.array.size() > 0 ? type.array[0] : 1;
			if (arraySize == 0)
			{
				arraySize = 1;
			}

			reflection.SampledImages.push_back({ name, descriptorSet, binding, dimension, arraySize });
		}

		for (const auto& resource : resources.separate_images)
		{
			const auto& name = resource.name;
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);

			uint32_t dimension = 0;
			switch (baseType.image.dim)
			{
				case spv::Dim::Dim1D:
					dimension = 1;
					break;
				case spv::Dim::Dim2D:
					dimension = 2;
					break;
				case spv::Dim::Dim3D:
				case spv::Dim::DimCube:
					dimension = 3;
					break;
			}
			uint32_t arraySize = type.array.size() > 0 ? type.array[0] : 1;
			if (arraySize == 0)
			{
				arraySize = 1;
			}

			reflection.SeparateImages.push_back({ name, descriptorSet, binding, dimension, arraySize });
		}

		return reflection;
	}

#ifdef DE_PLATFORM_WINDOWS
	// SM 5.0 (DX11) forbids dynamic (non-literal) indexing into resource arrays.
	// SPIRV-Cross emits "u_Textures[dynExpr].Sample(...)" which FXC rejects with X3512.
	// We inject a switch-based helper where every case uses a compile-time literal index,
	// which DX11 accepts, and replace all dynamic accesses with calls to that helper.
	static std::string PatchDX11ResourceArrayAccess(const std::string& hlsl)
	{
		std::string result = hlsl;
		std::string helpers;

		struct ArrayInfo { std::string name; std::string elemType; int size; };
		std::vector<ArrayInfo> arrays;

		static const std::string kPrefix = "Texture2D<";
		for (size_t p = 0; (p = result.find(kPrefix, p)) != std::string::npos; )
		{
			size_t tStart = p + kPrefix.size();
			size_t tEnd = result.find('>', tStart);
			if (tEnd == std::string::npos) { ++p; continue; }
			std::string elemType = result.substr(tStart, tEnd - tStart);

			size_t q = tEnd + 1;
			while (q < result.size() && result[q] == ' ') ++q;

			size_t nStart = q;
			while (q < result.size() && (isalnum((unsigned char)result[q]) || result[q] == '_')) ++q;
			if (q == nStart) { p = tEnd + 1; continue; }
			std::string name = result.substr(nStart, q - nStart);

			while (q < result.size() && result[q] == ' ') ++q;
			if (q >= result.size() || result[q] != '[') { p = q; continue; }
			++q;

			size_t sStart = q;
			while (q < result.size() && isdigit((unsigned char)result[q])) ++q;
			if (q == sStart || q >= result.size() || result[q] != ']') { p = q; continue; }

			arrays.push_back({ name, elemType, std::stoi(result.substr(sStart, q - sStart)) });
			p = q + 1;
		}

		for (const auto& arr : arrays)
		{
			const std::string arrPrefix = arr.name + "[";
			bool hasDynamic = false;
			for (size_t p = result.find(arrPrefix); p != std::string::npos; p = result.find(arrPrefix, p + 1))
			{
				size_t iStart = p + arrPrefix.size();
				size_t iEnd = result.find(']', iStart);
				if (iEnd == std::string::npos) continue;
				const std::string idx = result.substr(iStart, iEnd - iStart);
				bool isLit = !idx.empty() && std::all_of(idx.begin(), idx.end(), [](char c) { return isdigit((unsigned char)c) != 0; });
				if (!isLit) { hasDynamic = true; break; }
			}
			if (!hasDynamic) continue;

			const std::string fnName = arr.name + "_SampleByIndex";
			std::string fn = arr.elemType + " " + fnName + "(int _i, SamplerState _s, float2 _uv)\n{\n    switch (_i)\n    {\n";
			for (int i = 0; i < arr.size; ++i)
				fn += "        case " + std::to_string(i) + ": return " + arr.name + "[" + std::to_string(i) + "].Sample(_s, _uv);\n";
			fn += "        default: return " + arr.elemType + "(0, 0, 0, 0);\n    }\n}\n\n";
			helpers += fn;

			for (size_t p = result.find(arrPrefix); p != std::string::npos; )
			{
				size_t iStart = p + arrPrefix.size();
				size_t iEnd = result.find(']', iStart);
				if (iEnd == std::string::npos) break;

				const std::string idx = result.substr(iStart, iEnd - iStart);
				bool isLit = !idx.empty() && std::all_of(idx.begin(), idx.end(), [](char c) { return isdigit((unsigned char)c) != 0; });
				if (isLit || result.compare(iEnd + 1, 8, ".Sample(") != 0) { p = result.find(arrPrefix, iEnd + 1); continue; }

				// Walk balanced parens to find the end of .Sample(...)
				size_t argsStart = iEnd + 9; // skip "].Sample("
				int depth = 1;
				size_t q = argsStart;
				while (q < result.size() && depth > 0)
				{
					if (result[q] == '(') ++depth;
					else if (result[q] == ')') --depth;
					++q;
				}
				const std::string args = result.substr(argsStart, q - 1 - argsStart);
				const std::string repl = fnName + "(" + idx + ", " + args + ")";
				result.replace(p, q - p, repl);
				p = result.find(arrPrefix, p + repl.size());
			}
		}

		if (!helpers.empty())
		{
			size_t insertAt = result.find("\nvoid ");
			insertAt = (insertAt != std::string::npos) ? insertAt + 1 : 0;
			result.insert(insertAt, helpers);
		}

		return result;
	}
#endif

#ifdef DE_PLATFORM_WINDOWS
	static std::string StripNonUniformQualifier(const std::string& source)
	{
		std::string result = source;

		// Remove any "#extension GL_EXT_nonuniform_qualifier : ..." lines
		const std::string ext = "#extension GL_EXT_nonuniform_qualifier";
		for (size_t pos = result.find(ext); pos != std::string::npos; pos = result.find(ext))
		{
			size_t lineEnd = result.find('\n', pos);
			result.erase(pos, (lineEnd == std::string::npos ? result.size() : lineEnd + 1) - pos);
		}

		// Replace nonuniformEXT(expr) → expr by stripping the wrapper
		const std::string token = "nonuniformEXT(";
		for (size_t pos = result.find(token); pos != std::string::npos; pos = result.find(token, pos))
		{
			size_t innerStart = pos + token.size();
			int depth = 1;
			size_t i = innerStart;
			while (i < result.size() && depth > 0)
			{
				if (result[i] == '(') ++depth;
				else if (result[i] == ')') --depth;
				++i;
			}
			result.replace(pos, i - pos, result.substr(innerStart, i - 1 - innerStart));
		}

		return result;
	}
#endif

	std::vector<uint8_t> ShaderCompiler::CompileGLSLToHLSLBytecode(ShaderType shaderType, const std::string& source, const std::string& name, uint32_t shaderModel, bool assertOnFailure)
	{
#ifdef DE_PLATFORM_WINDOWS
		// SM 5.0 (DX11) has no NonUniformResourceIndex — strip the nonuniformEXT wrapper so
		// SPIRV-Cross can emit plain array indexing, which works on all DX11 hardware in practice.
		const std::string& processedSource = (shaderModel < 51) ? StripNonUniformQualifier(source) : source;

		// Step 1: GLSL → SPIR-V (use zero optimization to improve HLSL translation quality)
		auto spirv = CompileGLSL(shaderType, processedSource, name, "main", false, assertOnFailure);
		if (spirv.empty())
			return {};

		// Step 2: SPIR-V → HLSL via spirv-cross
		spirv_cross::CompilerHLSL hlslCompiler(spirv);
		spirv_cross::CompilerHLSL::Options hlslOptions;
		hlslOptions.shader_model = shaderModel;
		hlslOptions.point_size_compat = true;
		hlslCompiler.set_hlsl_options(hlslOptions);

		spirv_cross::CompilerGLSL::Options commonOptions;
		// Do NOT fix up clip space: for the HLSL backend, fixup_clipspace rewrites GL-style
		// [-w, w] depth to [0, w]. Our GLSL is compiled with GLM_FORCE_DEPTH_ZERO_TO_ONE, so the
		// SPIR-V already emits [0, w] (Vulkan/D3D) depth — re-applying the conversion corrupts the
		// depth values and breaks depth testing on D3D (2D is unaffected since it runs depth-less).
		commonOptions.vertex.fixup_clipspace = false;
		hlslCompiler.set_common_options(commonOptions);

		// Remap vertex input semantics to use the original GLSL variable names.
		// By default spirv-cross uses TEXCOORD0/1/2... but NVRHI's D3D12 input layout
		// uses the attribute name (e.g. "a_Position") as the D3D12 semantic — they must match.
		if (shaderType == ShaderType::Vertex)
		{
			spirv_cross::ShaderResources resources = hlslCompiler.get_shader_resources();
			for (const auto& input : resources.stage_inputs)
			{
				uint32_t location = hlslCompiler.get_decoration(input.id, spv::DecorationLocation);
				hlslCompiler.add_vertex_attribute_remap({ location, input.name });
			}
		}

		std::string hlslSource;
		try
		{
			hlslSource = hlslCompiler.compile();
		}
		catch (const spirv_cross::CompilerError& e)
		{
			DE_CORE_ERROR("spirv-cross HLSL compilation failed for '{}': {}", name, e.what());
			if (assertOnFailure)
			{
				DE_CORE_ASSERT(false, "spirv-cross failed to translate SPIR-V to HLSL.");
			}
			return {};
		}

		// SM 5.0 (DX11) rejects dynamic resource array indexing (error X3512).
		// Replace "array[dynExpr].Sample(...)" with a switch-based helper using literal indices.
		if (shaderModel < 51)
			hlslSource = PatchDX11ResourceArrayAccess(hlslSource);

		// Step 3: HLSL → DXBC via D3DCompile
		std::string target = Utils::GetHLSLTarget(shaderType, shaderModel);

		ID3DBlob* shaderBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;

		UINT compileFlags = 0;
#if defined(DE_DEBUG)
		compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		HRESULT hr = D3DCompile(
			hlslSource.c_str(), hlslSource.size(),
			name.c_str(), nullptr, nullptr,
			"main", target.c_str(),
			compileFlags, 0,
			&shaderBlob, &errorBlob
		);

		if (FAILED(hr))
		{
			if (errorBlob)
			{
				DE_CORE_ERROR("HLSL compilation error for '{}' (target={}): {}", name, target, (const char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			DE_CORE_ERROR("Generated HLSL source for '{}':\n{}", name, hlslSource);
			if (assertOnFailure)
			{
				DE_CORE_ASSERT(false, "HLSL shader compilation failed.");
			}
			return {};
		}

		if (errorBlob)
			errorBlob->Release();

		std::vector<uint8_t> bytecode(shaderBlob->GetBufferSize());
		std::memcpy(bytecode.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
		shaderBlob->Release();

		return bytecode;
#else
		DE_CORE_ASSERT(false, "HLSL shader compilation is only supported on Windows.");
		return {};
#endif
	}

	void ShaderCompiler::PrintReflection(ShaderType shaderType, const ShaderReflection& reflection) const
	{
		DE_CORE_TRACE("-------------------------------------------------------------");
		DE_CORE_TRACE("Shader Reflection for {0}:", Utils::ConvertShaderTypeToString(shaderType));
		DE_CORE_TRACE("  {0} uniform buffers", reflection.UniformBuffers.size());
		for (const auto& uniformBuffer : reflection.UniformBuffers)
		{
			DE_CORE_TRACE("    {0} (Set={1}, Binding={2})", uniformBuffer.Name, uniformBuffer.DescriptorSet, uniformBuffer.Binding);
		}

		DE_CORE_TRACE("  {0} storage buffers", reflection.StorageBuffers.size());
		for (const auto& storageBuffer : reflection.StorageBuffers)
		{
			DE_CORE_TRACE("    {0} (Set={1}, Binding={2})", storageBuffer.Name, storageBuffer.DescriptorSet, storageBuffer.Binding);
		}

		DE_CORE_TRACE("  {0} push constant buffers", reflection.PushConstantBuffers.size());
		for (const auto& pushConstant : reflection.PushConstantBuffers)
		{
			DE_CORE_TRACE("    {0} (Set={1}, Binding={2})", pushConstant.Name, pushConstant.DescriptorSet, pushConstant.Binding);
		}

		DE_CORE_TRACE("  {0} Separate Samplers", reflection.SeparateSamplers.size());
		for (const auto& separateSampler : reflection.SeparateSamplers)
		{
			DE_CORE_TRACE("    {0}	(Set={1}, Binding={2})", separateSampler.Name, separateSampler.DescriptorSet, separateSampler.Binding);
			DE_CORE_TRACE("			(Dimension={0}, ArraySize={1})", separateSampler.Dimension, separateSampler.ArraySize);
		}

		DE_CORE_TRACE("  {0} Sampled Images", reflection.SampledImages.size());
		for (const auto& sampledImage : reflection.SampledImages)
		{
			DE_CORE_TRACE("    {0}	(Set={1}, Binding={2})", sampledImage.Name, sampledImage.DescriptorSet, sampledImage.Binding);
			DE_CORE_TRACE("			(Dimension={0}, ArraySize={1})", sampledImage.Dimension, sampledImage.ArraySize);
		}

		DE_CORE_TRACE("  {0} Separate Images", reflection.SeparateImages.size());
		for (const auto& separateImage : reflection.SeparateImages)
		{
			DE_CORE_TRACE("    {0}	(Set={1}, Binding={2})", separateImage.Name, separateImage.DescriptorSet, separateImage.Binding);
			DE_CORE_TRACE("			(Dimension={0}, ArraySize={1})", separateImage.Dimension, separateImage.ArraySize);
		}
		DE_CORE_TRACE("-------------------------------------------------------------");
	}

} // namespace Dingo

