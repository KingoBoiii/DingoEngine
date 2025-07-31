#include "depch.h"
#include "ShaderCompiler.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

namespace Dingo
{

	namespace Utils
	{

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

	std::vector<uint32_t> ShaderCompiler::CompileGLSL(ShaderType shaderType, const std::string& source, const std::string& name, const std::string& entryPoint, bool optimize)
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
			DE_CORE_ASSERT(false);
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
			uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			int memberCount = bufferType.member_types.size();
			uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);

			reflection.PushConstantBuffers.push_back({ name, descriptorSet, binding });
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

			reflection.SeperateSamplers.push_back({ name, descriptorSet, binding, dimension, arraySize });
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

			reflection.SeperateImages.push_back({ name, descriptorSet, binding, dimension, arraySize });
		}

		return reflection;
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

		DE_CORE_TRACE("  {0} Seperate Samplers", reflection.SeperateSamplers.size());
		for (const auto& seperateSampler : reflection.SeperateSamplers)
		{
			DE_CORE_TRACE("    {0}	(Set={1}, Binding={2})", seperateSampler.Name, seperateSampler.DescriptorSet, seperateSampler.Binding);
			DE_CORE_TRACE("			(Dimension={0}, ArraySize={1})", seperateSampler.Dimension, seperateSampler.ArraySize);
		}

		DE_CORE_TRACE("  {0} Sampled Images", reflection.SampledImages.size());
		for (const auto& sampledImage : reflection.SampledImages)
		{
			DE_CORE_TRACE("    {0}	(Set={1}, Binding={2})", sampledImage.Name, sampledImage.DescriptorSet, sampledImage.Binding);
			DE_CORE_TRACE("			(Dimension={0}, ArraySize={1})", sampledImage.Dimension, sampledImage.ArraySize);
		}

		DE_CORE_TRACE("  {0} Seperate Images", reflection.SeperateImages.size());
		for (const auto& seperateImage : reflection.SeperateImages)
		{
			DE_CORE_TRACE("    {0}	(Set={1}, Binding={2})", seperateImage.Name, seperateImage.DescriptorSet, seperateImage.Binding);
			DE_CORE_TRACE("			(Dimension={0}, ArraySize={1})", seperateImage.Dimension, seperateImage.ArraySize);
		}
		DE_CORE_TRACE("-------------------------------------------------------------");
	}

} // namespace Dingo

