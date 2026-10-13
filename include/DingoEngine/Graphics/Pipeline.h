#pragma once
#include "Shader.h"
#include "Framebuffer.h"
#include "Enums.h"
#include "GraphicsBuffer.h"
#include "Texture.h"

namespace Dingo
{

	/// One vertex attribute within a VertexLayout.
	struct VertexLayoutAttribute
	{
		/// Attribute name; on Vulkan its shader location is its position in Attributes.
		std::string Name;
		Dingo::Format Format; ///< Data format of the attribute; required.
		uint32_t Offset = 0; ///< Byte offset of the attribute within one vertex.
	};

	/// How one vertex buffer's bytes map to a vertex shader's inputs.
	struct VertexLayout
	{
		uint32_t Stride = 0; ///< Size of one vertex in bytes.
		/// The attributes, in the shader's input order. Empty for a vertex stage that reads no
		/// buffer.
		std::vector<VertexLayoutAttribute> Attributes;

		/// Sets Stride, the size of one vertex in bytes, and returns *this.
		VertexLayout& SetStride(uint32_t stride)
		{
			Stride = stride;
			return *this;
		}

		/// Appends an attribute at the given byte offset and returns *this.
		VertexLayout& AddAttribute(const std::string& name, Format format, uint32_t offset)
		{
			Attributes.push_back({ name, format, offset });
			return *this;
		}
	};

	/// Describes the fixed-function state of a Pipeline for Pipeline::Create.
	struct PipelineParams
	{
		std::string DebugName; ///< Name shown in graphics debuggers and logs.
		Dingo::Shader* Shader = nullptr; ///< Required; borrowed, must outlive the pipeline.
		/// Required: the target the pipeline is built against, and only guaranteed live at Create.
		/// The swap chain's framebuffer is re-resolved on a rebuild; any other must outlive the
		/// pipeline.
		Dingo::Framebuffer* Framebuffer = nullptr;
		Dingo::FillMode FillMode = Dingo::FillMode::Solid; ///< Filled triangles or wireframe.
		Dingo::CullMode CullMode = Dingo::CullMode::Back; ///< Which faces are discarded.
		/// Whether counter-clockwise triangles are the front faces.
		bool FrontCounterClockwise = true;
		/// only takes effect if the target framebuffer has a depth attachment
		bool DepthTest = true;
		bool DepthWrite = true;  ///< 2D/overlay pipelines should set both false (painter's order)
		/// The depth test, when DepthTest is on.
		Dingo::DepthCompare DepthCompare = Dingo::DepthCompare::Less;
		/// How colour combines with the target's.
		Dingo::BlendMode BlendMode = Dingo::BlendMode::Alpha;
		/// Pushes the stored depth away from the viewer, for shadow maps: DepthBias in units of the
		/// smallest depth step, SlopeScaledDepthBias times the triangle's depth slope. Vulkan
		/// applies either only while DepthBias is not 0.
		int32_t DepthBias = 0;
		float SlopeScaledDepthBias = 0.0f; ///< See DepthBias.
		Dingo::VertexLayout VertexLayout; ///< The vertex buffer layout the vertex stage reads.
		/// Optional uniform buffer put in the pipeline's own binding set at binding 0. It is
		/// volatile, so write it in every frame that binds it. Materials and RenderPass set theirs
		/// themselves.
		GraphicsBuffer* UniformBuffer = nullptr;
		/// Optional texture put in the pipeline's own binding set at binding 1, with the renderer's
		/// clamp sampler at binding 2. Materials and RenderPass bind their own.
		Dingo::Texture* Texture = nullptr;

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		PipelineParams& SetDebugName(const std::string& debugName)
		{
			DebugName = debugName;
			return *this;
		}

		PipelineParams& SetShader(Dingo::Shader* shader)
		{
			Shader = shader;
			return *this;
		}

		PipelineParams& SetFramebuffer(Dingo::Framebuffer* framebuffer)
		{
			Framebuffer = framebuffer;
			return *this;
		}

		PipelineParams& SetFillMode(Dingo::FillMode fillMode)
		{
			FillMode = fillMode;
			return *this;
		}

		PipelineParams& SetCullMode(Dingo::CullMode cullMode)
		{
			CullMode = cullMode;
			return *this;
		}

		PipelineParams& SetFrontCounterClockwise(bool frontCounterClockwise)
		{
			FrontCounterClockwise = frontCounterClockwise;
			return *this;
		}

		PipelineParams& SetDepthTest(bool enabled)
		{
			DepthTest = enabled;
			return *this;
		}

		PipelineParams& SetDepthWrite(bool enabled)
		{
			DepthWrite = enabled;
			return *this;
		}

		PipelineParams& SetDepthCompare(Dingo::DepthCompare compare)
		{
			DepthCompare = compare;
			return *this;
		}

		PipelineParams& SetBlendMode(Dingo::BlendMode blendMode)
		{
			BlendMode = blendMode;
			return *this;
		}

		///@}

		/// Sets DepthBias to constant and SlopeScaledDepthBias to slopeScaled, and returns *this.
		PipelineParams& SetDepthBias(int32_t constant, float slopeScaled)
		{
			DepthBias = constant;
			SlopeScaledDepthBias = slopeScaled;
			return *this;
		}

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
		PipelineParams& SetVertexLayout(const Dingo::VertexLayout& vertexLayout)
		{
			VertexLayout = vertexLayout;
			return *this;
		}

		PipelineParams& SetUniformBuffer(Dingo::GraphicsBuffer* uniformBuffer)
		{
			UniformBuffer = uniformBuffer;
			return *this;
		}

		PipelineParams& SetTexture(Dingo::Texture* texture)
		{
			Texture = texture;
			return *this;
		}
		///@}
	};

	/// A compiled graphics pipeline: shader, vertex layout and fixed-function state for one target.
	/// The caller owns it and delete frees it; it rebuilds itself after a shader reload.
	class Pipeline
	{
	public:
		/// Builds the pipeline and returns it ready to bind. The caller owns the result.
		[[nodiscard]] static Pipeline* Create(const PipelineParams& params);

	public:
		/// For subclasses; make pipelines with Create.
		Pipeline(const PipelineParams& params)
			: m_Params(params)
		{}
		/// Frees the GPU pipeline.
		virtual ~Pipeline() = default;

	protected:
		/// Create() calls it, so a resource is ready when the factory returns it.
		virtual void Initialize() = 0;

	public:
		/// Releases the GPU object early; the destructor releases everything anyway.
		virtual void Destroy() = 0;

		/// The params the pipeline was created with.
		const PipelineParams& GetParams() const { return m_Params; }
		/// The framebuffer the pipeline draws into.
		Framebuffer* GetTargetFramebuffer() const { return m_Params.Framebuffer; }

	protected:
		PipelineParams m_Params; ///< The params the pipeline was created with.
	};

}
