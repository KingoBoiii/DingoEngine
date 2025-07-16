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

		static std::string ShaderTypeToString(ShaderType shaderType)
		{
			switch (shaderType)
			{
				case ShaderType::Vertex: return "Vertex";
				case ShaderType::Fragment: return "Fragment";
				case ShaderType::Geometry: return "Geometry";
				case ShaderType::Compute: return "Compute";
				default: return "Unknown";
			}
		}

	}

	std::unordered_map<ShaderType, std::vector<uint32_t>> ShaderCompiler::CompileGLSL(std::unordered_map<ShaderType, std::string> sources, const std::string& name, bool optimize)
	{
		shaderc::Compiler compiler;
		shaderc::CompileOptions compileOptions;
		compileOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
		compileOptions.SetOptimizationLevel(optimize ? shaderc_optimization_level_performance : shaderc_optimization_level_zero);

		std::unordered_map<ShaderType, std::vector<uint32_t>> resultSources;

		for (auto& [shaderType, source] : sources)
		{
			shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, Utils::ConvertShaderTypeToShaderC(shaderType), name.c_str(), "main", compileOptions);
			if (result.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				DE_CORE_ERROR("Shader compilation failed: {}", result.GetErrorMessage());
				DE_CORE_ASSERT(false);
			}

			resultSources[shaderType] = std::vector<uint32_t>(result.begin(), result.end());
		}

		return resultSources;
	}

	std::vector<uint32_t> ShaderCompiler::CompileGLSL(ShaderType shaderType, const std::string& source, const std::string& name, bool optimize)
	{
		shaderc::Compiler compiler;
		shaderc::CompileOptions compileOptions;
		compileOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
		compileOptions.SetOptimizationLevel(optimize ? shaderc_optimization_level_performance : shaderc_optimization_level_zero);

		shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, Utils::ConvertShaderTypeToShaderC(shaderType), name.c_str(), "main", compileOptions);
		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			DE_CORE_ERROR("Shader compilation failed: {}", result.GetErrorMessage());
			DE_CORE_ASSERT(false);
		}

		return std::vector<uint32_t>(result.begin(), result.end());
	}

	void ShaderCompiler::Reflect(ShaderType shaderType, const std::vector<uint32_t> binaries)
	{
		spirv_cross::Compiler compiler(binaries);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		DE_CORE_TRACE("Shader Reflection for {0}:", Utils::ShaderTypeToString(shaderType));
		DE_CORE_TRACE("    {0} uniform buffers", resources.uniform_buffers.size());
		DE_CORE_TRACE("    {0} storage buffers", resources.storage_buffers.size());
		DE_CORE_TRACE("    {0} push constant buffers", resources.push_constant_buffers.size());
		DE_CORE_TRACE("    {0} Sampled images", resources.sampled_images.size());
		DE_CORE_TRACE("    {0} Separate images", resources.separate_images.size());
		DE_CORE_TRACE("    {0} Separate samplers", resources.separate_samplers.size());

		DE_CORE_TRACE("Uniform buffers:");
		for (const auto& resource : resources.uniform_buffers)
		{
			const auto& name = resource.name;
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			int memberCount = bufferType.member_types.size();
			uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);

			DE_CORE_TRACE("  {0} ({1}, {2})", name, descriptorSet, binding);
			DE_CORE_TRACE("  Member Count: {0}", memberCount);
			DE_CORE_TRACE("  Size: {0} bytes", size);
			DE_CORE_TRACE("-------------------");
		}

		DE_CORE_TRACE("Storage buffers:");
		for (const auto& resource : resources.storage_buffers)
		{
			const auto& name = resource.name;
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			int memberCount = bufferType.member_types.size();
			uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);

			DE_CORE_TRACE("  {0} ({1}, {2})", name, descriptorSet, binding);
			DE_CORE_TRACE("  Member Count: {0}", memberCount);
			DE_CORE_TRACE("  Size: {0} bytes", size);
			DE_CORE_TRACE("-------------------");
		}

		DE_CORE_TRACE("Push Constant buffers:");
		for (const auto& resource : resources.push_constant_buffers)
		{
			const auto& name = resource.name;
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			int memberCount = bufferType.member_types.size();
			uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);

			DE_CORE_TRACE("  {0} ({1}, {2})", name, descriptorSet, binding);
			DE_CORE_TRACE("  Member Count: {0}", memberCount);
			DE_CORE_TRACE("  Size: {0} bytes", size);
			DE_CORE_TRACE("-------------------");
		}

		DE_CORE_TRACE("Sampled images:");
		for (const auto& resource : resources.sampled_images)
		{
			const auto& name = resource.name;
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);

			DE_CORE_TRACE("  {0} ({1}, {2})", name, descriptorSet, binding);
		}

		DE_CORE_TRACE("Separate Images:");
		for (const auto& resource : resources.separate_images)
		{
			const auto& name = resource.name;
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);

			DE_CORE_TRACE("  {0} ({1}, {2})", name, descriptorSet, binding);
		}
	}

} // namespace Dingo

