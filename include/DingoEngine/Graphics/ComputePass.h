#pragma once
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/Sampler.h"
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/Graphics/Texture.h"

#include <string>

namespace Dingo
{

	struct ComputePassParams
	{
		std::string DebugName;
		// A shader with a "#type compute" stage and nothing else.
		Dingo::Shader* Shader = nullptr;

		ComputePassParams& SetDebugName(const std::string& name)
		{
			DebugName = name;
			return *this;
		}

		ComputePassParams& SetShader(Dingo::Shader* shader)
		{
			Shader = shader;
			return *this;
		}
	};

	// A compute shader and what it binds, run with Renderer::Dispatch. Bindings are the shader's own
	// GLSL binding numbers; setting a binding again replaces it. Storage buffers and images bind as the
	// shader declares them: readonly as shader-resource views, the rest writable. NVRHI orders a
	// dispatch against the draws and dispatches around it, so a buffer a dispatch writes can be read
	// by the next draw. D3D11 has 8 writable slots in compute: keep writable bindings below 8. The
	// pipeline and bindings rebuild when the shader hot-reloads or a bound texture is reinitialized.
	class ComputePass
	{
	public:
		static ComputePass* Create(const ComputePassParams& params);

	public:
		ComputePass(const ComputePassParams& params)
			: m_Params(params)
		{}
		virtual ~ComputePass() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;

		virtual void SetUniformBuffer(uint32_t binding, GraphicsBuffer* buffer) = 0;
		virtual void SetStorageBuffer(uint32_t binding, GraphicsBuffer* buffer) = 0;
		virtual void SetTexture(uint32_t binding, Texture* texture) = 0;
		// A texture made with TextureParams::IsStorage, as an image the shader writes.
		virtual void SetStorageTexture(uint32_t binding, Texture* texture) = 0;
		virtual void SetSampler(uint32_t binding, Sampler* sampler) = 0;

		const ComputePassParams& GetParams() const { return m_Params; }

	protected:
		ComputePassParams m_Params;
	};

}
