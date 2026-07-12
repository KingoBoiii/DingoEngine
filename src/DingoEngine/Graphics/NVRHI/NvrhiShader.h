#pragma once
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/ShaderCompiler.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiShader : public Shader
	{
	public:
		NvrhiShader(const ShaderParams& params)
			: Shader(params)
		{}
		~NvrhiShader() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;
		virtual bool IsValid() const override { return !m_ShaderHandles.empty(); }
		virtual bool Reload() override;

	private:
		// Compiles the sources and creates all handles, committing them to the members
		// only when everything succeeded. forceCompile bypasses the name-keyed bytecode
		// disk caches (rewriting them on success); tolerateErrors downgrades compile
		// failures from asserts to a false return, leaving the old program in place.
		bool Build(bool forceCompile, bool tolerateErrors);

		nvrhi::ShaderHandle CreateShaderHandle(nvrhi::ShaderType shaderType, const std::vector<uint32_t>& spvbinary, const std::string& debugName = "Shader");
		nvrhi::BindingLayoutHandle CreateBindingLayoutHandle(const std::vector<ShaderReflection>& reflection);

		// Freshly-compiled bytecode is appended to pendingCacheWrites instead of hitting
		// disk here, so a failed multi-stage build never leaves mixed old/new cache files.
		std::unordered_map<ShaderType, std::vector<uint32_t>> CompileOrGetShaderBinaries(const std::unordered_map<ShaderType, std::string>& sources, const std::string& name, ShaderCompiler& compiler, bool forceCompile, bool tolerateErrors, std::vector<std::pair<std::filesystem::path, std::string>>& pendingCacheWrites);
		std::unordered_map<ShaderType, std::string> GetShaderSources() const;
		std::unordered_map<ShaderType, std::string> PreProcess(const std::string& source) const;

	private:
		std::unordered_map<ShaderType, nvrhi::ShaderHandle> m_ShaderHandles;
		nvrhi::BindingLayoutHandle m_BindingLayoutHandle;

		friend class NvrhiPipeline;
		friend class NvrhiRenderPass;
		friend class ImGuiRenderer;
	};

}
