#include "depch.h"
#include "DingoEngine/Graphics/Renderer2D.h"

#include "MSDFData.h"

#include "DingoEngine/Core/Application.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Dingo
{

	namespace Shaders
	{

		constexpr const char* Renderer2DQuadShader = R"(
#type vertex
#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;

layout (std140, binding = 0) uniform Camera {
	mat4 ProjectionView;
};

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out flat float v_TexIndex;

void main()
{
	gl_Position = ProjectionView * vec4(a_Position, 1.0);
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
	v_TexIndex = a_TexIndex;
}

#type fragment
#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in flat float v_TexIndex;

layout(location = 0) out vec4 o_Color;

layout (set = 0, binding = 1) uniform texture2D u_Textures[32];
layout (set = 0, binding = 2) uniform sampler u_Sampler;

void main()
{
	o_Color = texture(sampler2D(u_Textures[nonuniformEXT(int(v_TexIndex))], u_Sampler), v_TexCoord) * v_Color;

	if (o_Color.a == 0.0)
	{
		discard; // Skip rendering if the color is fully transparent
	}
}
		)";

		constexpr const char* Renderer2DCircleShader = R"(
#type vertex
#version 450 core

layout(location = 0) in vec3 a_WorldPosition;
layout(location = 1) in vec3 a_LocalPosition;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in float a_Thickness;
layout(location = 4) in float a_Fade;

layout (std140, binding = 0) uniform Camera {
	mat4 ProjectionView;
};

struct VertexOutput
{
	vec3 LocalPosition;
	vec4 Color;
	float Thickness;
	float Fade;
};

layout (location = 0) out VertexOutput Output;

void main()
{
	Output.LocalPosition = a_LocalPosition;
	Output.Color = a_Color;
	Output.Thickness = a_Thickness;
	Output.Fade = a_Fade;

	gl_Position = ProjectionView * vec4(a_WorldPosition, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

struct VertexOutput
{
	vec3 LocalPosition;
	vec4 Color;
	float Thickness;
	float Fade;
};

layout (location = 0) in VertexOutput Input;

void main()
{
    // Calculate distance and fill circle with white
    float distance = 1.0 - length(Input.LocalPosition);
    float circle = smoothstep(0.0, Input.Fade, distance);
    circle *= smoothstep(Input.Thickness + Input.Fade, Input.Thickness, distance);

	if (circle == 0.0) 
	{
		discard;
	}

    // Set output color
    o_Color = Input.Color;
	o_Color.a *= circle;
}
)";

		constexpr const char* Renderer2DTextShader = R"(
#type vertex
#version 450
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;

layout (std140, binding = 0) uniform Camera {
	mat4 ProjectionView;
};

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec2 v_TexCoord;

void main()
{
	gl_Position = ProjectionView * vec4(a_Position, 1.0);
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
}

#type fragment
#version 450
layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;

layout(location = 0) out vec4 o_Color;

layout (set = 0, binding = 1) uniform texture2D u_AtlasTexture;
layout (set = 0, binding = 2) uniform sampler u_Sampler;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

/* For 2D
float ScreenPxRange()
{
	float pixRange = 2.0f;
	float geoSize = 72.0f;
	return geoSize / 32.0f * pixRange;
}
*/

float ScreenPxRange()
{
	float pxRange = 2.0f;
    vec2 unitRange = vec2(pxRange) / vec2(textureSize(sampler2D(u_AtlasTexture, u_Sampler), 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(v_TexCoord);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
	vec4 bgColor = vec4(v_Color.rgb, 0.0);
	vec4 fgColor = v_Color;

	vec3 msd = texture(sampler2D(u_AtlasTexture, u_Sampler), v_TexCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = ScreenPxRange() * (sd - 0.5f);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    o_Color = mix(bgColor, fgColor, opacity);
}
		)";

	}

	namespace Utils
	{

		// Rotation and scaling are applied in the order: translate -> rotate -> scale
		// This is the same order as glm::translate * glm::rotate * glm::scale
		// Note: rotation is in degrees
		inline static glm::mat4 CreateTransform(const glm::vec3& position, const glm::vec2& size, float rotation = 0.0f)
		{
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);

			if (rotation != 0.0f)
			{
				transform *= glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0.0f, 0.0f, 1.0f });
			}

			transform *= glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

			return transform;
		}

	}

	Renderer2D* Renderer2D::Create(const Renderer2DCapabilities& capabilities)
	{
		Renderer2D* renderer2D = new Renderer2D(Renderer2DParams{ .Capabilities = capabilities });
		renderer2D->Initialize();
		return renderer2D;
	}

	Renderer2D* Renderer2D::Create(const Renderer2DParams& params)
	{
		Renderer2D* renderer2D = new Renderer2D(params);
		renderer2D->Initialize();
		return renderer2D;
	}

	void Renderer2D::Initialize()
	{
		// A batch must hold at least one quad. A MaxQuads of 0 (e.g. supplied via
		// ApplicationParams) would size the batch buffers to zero and overflow on the
		// first Draw, so clamp it to a usable minimum.
		DE_CORE_ASSERT(m_Params.Capabilities.MaxQuads >= 1, "Renderer2D: MaxQuads must be >= 1");
		if (m_Params.Capabilities.MaxQuads < 1)
			m_Params.Capabilities.MaxQuads = 1;

		CreateQuadIndexBuffer();

		m_QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
		m_QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

		m_CameraUniformBuffer = GraphicsBuffer::CreateUniformBuffer(sizeof(CameraData));

		// Set all texture slots to 0
		m_TextureSlots[0] = Renderer::GetWhiteTexture();
		for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
		{
			m_TextureSlots[i] = nullptr;
		}

		CreateQuadPipeline();
		CreateCircleRenderPass();
		CreateTextQuadRenderPass();
	}

	void Renderer2D::Shutdown()
	{
		DestroyTextQuadRenderPass();
		DestroyCircleRenderPass();
		DestroyQuadPipeline();

		if (m_QuadIndexBuffer)
		{
			m_QuadIndexBuffer->Destroy();
			m_QuadIndexBuffer = nullptr;
		}

		if (m_CameraUniformBuffer)
		{
			m_CameraUniformBuffer->Destroy();
			m_CameraUniformBuffer = nullptr;
		}

	}

	void Renderer2D::BeginScene(const glm::mat4& projectionViewMatrix)
	{
		m_CameraData.ProjectionViewMatrix = projectionViewMatrix;
		m_CameraUniformBuffer->Upload(&m_CameraData, sizeof(CameraData));

		// Record the camera upload at the start of the scene so it precedes every
		// draw. Batches can now be flushed mid-frame (inside Draw* calls), not only
		// in EndScene, and each draw must observe the camera data.
		Renderer::Upload(m_CameraUniformBuffer);

		m_Statistics = {};

		m_QuadPipeline.IndexCount = 0;
		m_QuadPipeline.VertexBufferPtr = m_QuadPipeline.VertexBufferBase;
		m_QuadPipeline.BatchIndex = 0;

		m_CircleRenderPass.IndexCount = 0;
		m_CircleRenderPass.VertexBufferPtr = m_CircleRenderPass.VertexBufferBase;
		m_CircleRenderPass.BatchIndex = 0;

		m_TextQuadRenderPass.IndexCount = 0;
		m_TextQuadRenderPass.VertexBufferPtr = m_TextQuadRenderPass.VertexBufferBase;
		m_TextQuadRenderPass.BatchIndex = 0;

		ResetQuadTextureSlots();
	}

	void Renderer2D::EndScene()
	{
		// Submit whatever each pass has accumulated since its last flush. The bulk
		// of the work for large scenes already happened in mid-frame flushes; these
		// just drain the final partial batch (no-op when empty).
		FlushQuad();
		FlushCircle();
		FlushText();
	}

	void Renderer2D::Clear(const glm::vec4& clearColor)
	{
		m_Params.ClearColor = clearColor;
		Renderer::Clear(clearColor);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad(glm::vec3(position, 0.0f), size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		if (m_QuadPipeline.IndexCount + 6 > m_Params.Capabilities.GetQuadIndexCount())
			FlushQuad();

		constexpr size_t quadVertexCount = 4;

		glm::mat4 transform = Utils::CreateTransform(position, size);

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadPipeline.VertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadPipeline.VertexBufferPtr->Color = color;
			m_QuadPipeline.VertexBufferPtr->TexCoord = m_TextureCoords[i];
			m_QuadPipeline.VertexBufferPtr->TexIndex = 0.0f;
			m_QuadPipeline.VertexBufferPtr++;
		}

		m_QuadPipeline.IndexCount += 6;
		++m_Statistics.QuadCount;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		DrawQuad(glm::vec3(position, 0.0f), size, texture, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		if (m_QuadPipeline.IndexCount + 6 > m_Params.Capabilities.GetQuadIndexCount())
			FlushQuad();

		float textureIndex = GetTextureIndex(texture);

		constexpr size_t quadVertexCount = 4;

		glm::mat4 transform = Utils::CreateTransform(position, size);

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadPipeline.VertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadPipeline.VertexBufferPtr->Color = color;
			m_QuadPipeline.VertexBufferPtr->TexCoord = m_TextureCoords[i];
			m_QuadPipeline.VertexBufferPtr->TexIndex = textureIndex;
			m_QuadPipeline.VertexBufferPtr++;
		}

		m_QuadPipeline.IndexCount += 6;
		++m_Statistics.QuadCount;
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, float rotation, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		DrawRotatedQuad(glm::vec3(position, 0.0f), rotation, size, texture, color);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, float rotation, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		if (m_QuadPipeline.IndexCount + 6 > m_Params.Capabilities.GetQuadIndexCount())
			FlushQuad();

		float textureIndex = GetTextureIndex(texture);

		constexpr size_t quadVertexCount = 4;

		glm::mat4 transform = Utils::CreateTransform(position, size, rotation);

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadPipeline.VertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadPipeline.VertexBufferPtr->Color = color;
			m_QuadPipeline.VertexBufferPtr->TexCoord = m_TextureCoords[i];
			m_QuadPipeline.VertexBufferPtr->TexIndex = textureIndex;
			m_QuadPipeline.VertexBufferPtr++;
		}

		m_QuadPipeline.IndexCount += 6;
		++m_Statistics.QuadCount;
	}

	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade)
	{
		if (m_CircleRenderPass.IndexCount + 6 > m_Params.Capabilities.GetQuadIndexCount())
			FlushCircle();

		for (size_t i = 0; i < 4; i++)
		{
			m_CircleRenderPass.VertexBufferPtr->WorldPosition = transform * m_QuadVertexPositions[i];
			m_CircleRenderPass.VertexBufferPtr->LocalPosition = m_QuadVertexPositions[i] * 2.0f;
			m_CircleRenderPass.VertexBufferPtr->Color = color;
			m_CircleRenderPass.VertexBufferPtr->Thickness = thickness;
			m_CircleRenderPass.VertexBufferPtr->Fade = fade;
			m_CircleRenderPass.VertexBufferPtr++;
		}

		m_CircleRenderPass.IndexCount += 6;
		++m_Statistics.CircleCount;
	}

	void Renderer2D::DrawText(const std::string& string, const Font* font, const glm::vec2& position, float size, const TextParameters& textParameters)
	{
		DrawText(string, font, glm::vec3(position, 0.0f), size, textParameters);
	}

	void Renderer2D::DrawText(const std::string& string, const Font* font, const glm::vec3& position, float size, const TextParameters& textParameters)
	{
		const auto& fontGeometry = font->GetMSDFData()->FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();
		auto fontAtlas = font->GetAtlasTexture();

		glm::mat4 transform = Utils::CreateTransform(position, glm::vec2(size, size));

		m_FontAtlasTexture = fontAtlas;

		double x = 0.0;
		double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
		double y = 0.0f; // -fsScale * metrics.ascenderY;
		//double y = -fsScale * metrics.ascenderY;

		float spaceGlyphAdvance = fontGeometry.getGlyph(' ')->getAdvance();

		for (size_t i = 0; i < string.size(); i++)
		{
			char character = string[i];

			if (character == '\n')
			{
				x = 0.0;
				y -= fsScale * metrics.lineHeight + textParameters.LineSpacing;
				continue;
			}

			if (character == ' ')
			{
				float advance = spaceGlyphAdvance;
				if (i < string.size() - 1)
				{
					char nextCharacter = string[i + 1];
					double dAdvance;
					fontGeometry.getAdvance(dAdvance, character, nextCharacter);
					advance = (float)dAdvance;
				}
				x += fsScale * advance + textParameters.Kerning;
				continue;
			}

			if (character == '\t')
			{
				x += 4.0f * (fsScale * spaceGlyphAdvance + textParameters.Kerning);
				continue;
			}

			auto glyph = fontGeometry.getGlyph(character);
			if (!glyph)
			{
				glyph = fontGeometry.getGlyph('?');
			}
			if (!glyph)
			{
				return;
			}

			double al, ab, ar, at;
			glyph->getQuadAtlasBounds(al, ab, ar, at);
			glm::vec2 texCoordMin((float)al, (float)ab);
			glm::vec2 texCoordMax((float)ar, (float)at);

			double pl, pb, pr, pt;
			glyph->getQuadPlaneBounds(pl, pb, pr, pt);
			glm::vec2 quadMin((float)pl, (float)pb);
			glm::vec2 quadMax((float)pr, (float)pt);

			quadMin *= fsScale, quadMax *= fsScale;
			quadMin += glm::vec2(x, y);
			quadMax += glm::vec2(x, y);

			float texelWidth = 1.0f / fontAtlas->GetWidth();
			float texelHeight = 1.0f / fontAtlas->GetHeight();
			texCoordMin *= glm::vec2(texelWidth, texelHeight);
			texCoordMax *= glm::vec2(texelWidth, texelHeight);

			if (m_TextQuadRenderPass.IndexCount + 6 > m_Params.Capabilities.GetQuadIndexCount())
				FlushText();

			m_TextQuadRenderPass.VertexBufferPtr->Position = transform * glm::vec4(quadMin, 0.0f, 1.0f);
			m_TextQuadRenderPass.VertexBufferPtr->Color = textParameters.Color;
			m_TextQuadRenderPass.VertexBufferPtr->TexCoord = texCoordMin;
			m_TextQuadRenderPass.VertexBufferPtr++;

			m_TextQuadRenderPass.VertexBufferPtr->Position = transform * glm::vec4(quadMin.x, quadMax.y, 0.0f, 1.0f);
			m_TextQuadRenderPass.VertexBufferPtr->Color = textParameters.Color;
			m_TextQuadRenderPass.VertexBufferPtr->TexCoord = { texCoordMin.x, texCoordMax.y };
			m_TextQuadRenderPass.VertexBufferPtr++;

			m_TextQuadRenderPass.VertexBufferPtr->Position = transform * glm::vec4(quadMax, 0.0f, 1.0f);
			m_TextQuadRenderPass.VertexBufferPtr->Color = textParameters.Color;
			m_TextQuadRenderPass.VertexBufferPtr->TexCoord = texCoordMax;
			m_TextQuadRenderPass.VertexBufferPtr++;

			m_TextQuadRenderPass.VertexBufferPtr->Position = transform * glm::vec4(quadMax.x, quadMin.y, 0.0f, 1.0f);
			m_TextQuadRenderPass.VertexBufferPtr->Color = textParameters.Color;
			m_TextQuadRenderPass.VertexBufferPtr->TexCoord = { texCoordMax.x, texCoordMin.y };
			m_TextQuadRenderPass.VertexBufferPtr++;

			m_TextQuadRenderPass.IndexCount += 6;
			++m_Statistics.TextQuadCount;

			if (i < string.size() - 1)
			{
				double advance = glyph->getAdvance();
				char nextCharacter = string[i + 1];
				fontGeometry.getAdvance(advance, character, nextCharacter);

				x += fsScale * advance + textParameters.Kerning;
			}
		}
	}

	float Renderer2D::GetTextureIndex(Texture* texture)
	{
		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			if (m_TextureSlots[i]->NativeEquals(texture))
				return (float)i;
		}

		// This batch has no free texture slot for a new texture — flush it (which
		// resets the slots) and start the texture set over in a fresh batch.
		if (m_TextureSlotIndex >= MaxTextureSlots)
			FlushQuad();

		float textureIndex = (float)m_TextureSlotIndex;
		m_TextureSlots[m_TextureSlotIndex] = texture;
		m_TextureSlotIndex++;
		return textureIndex;
	}

	void Renderer2D::CreateQuadIndexBuffer()
	{
		// 32-bit indices: a single static index buffer shared by every batch of all
		// three passes. Index values are batch-local (0..MaxQuads*4), so uint32 is
		// far more than needed, but it removes the old ~16k-quad uint16 ceiling and
		// matches the engine's other uint32 index buffers (e.g. Breakout3D).
		uint32_t* quadIndices = new uint32_t[m_Params.Capabilities.GetQuadIndexCount()];

		uint32_t offset = 0;
		for (uint32_t i = 0; i < m_Params.Capabilities.GetQuadIndexCount(); i += 6)
		{
			quadIndices[i + 0] = offset + 0;
			quadIndices[i + 1] = offset + 1;
			quadIndices[i + 2] = offset + 2;

			quadIndices[i + 3] = offset + 2;
			quadIndices[i + 4] = offset + 3;
			quadIndices[i + 5] = offset + 0;

			offset += 4;
		}

		m_QuadIndexBuffer = GraphicsBuffer::CreateIndexBuffer(sizeof(uint32_t) * m_Params.Capabilities.GetQuadIndexCount(), quadIndices, true, "Renderer2DQuadIndexBuffer", GraphicsFormat::Uint32);

		delete[] quadIndices;
	}

	/**************************************************
	***		QUAD									***
	**************************************************/

	void Renderer2D::CreateQuadPipeline()
	{
		m_QuadPipeline = QuadPipeline();

		m_QuadPipeline.Shader = Shader::CreateFromSource("Renderer2DQuadShader", Shaders::Renderer2DQuadShader);

		VertexLayout vertexLayout = VertexLayout()
			.SetStride(sizeof(QuadVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(QuadVertex, Position))
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(QuadVertex, Color))
			.AddAttribute("a_TexCoord", Format::RG32_FLOAT, offsetof(QuadVertex, TexCoord))
			.AddAttribute("a_TexIndex", Format::R32_FLOAT, offsetof(QuadVertex, TexIndex));

		m_QuadPipeline.Pipeline = Pipeline::Create(PipelineParams()
			.SetDebugName("Renderer2DQuadPipeline")
			.SetFramebuffer(Renderer::GetSwapChainFramebuffer())
			.SetShader(m_QuadPipeline.Shader)
			.SetVertexLayout(vertexLayout)
			.SetCullMode(CullMode::BackAndFront)
			.SetDepthTest(false)
			.SetDepthWrite(false));

		// Per-batch vertex buffers and render passes are created lazily in FlushQuad.
		m_QuadPipeline.VertexBufferBase = new QuadVertex[m_Params.Capabilities.GetQuadVertexCount()];
	}

	RenderPass* Renderer2D::CreateQuadRenderPass()
	{
		RenderPass* renderPass = RenderPass::Create(RenderPassParams().SetPipeline(m_QuadPipeline.Pipeline));
		renderPass->Initialize();
		renderPass->SetUniformBuffer(0, m_CameraUniformBuffer);
		renderPass->SetSampler(2, Renderer::GetPointSampler());
		// The texture-slot array is bound and the binding set baked per batch in
		// FlushQuad, because each batch carries a different set of textures.
		return renderPass;
	}

	GraphicsBuffer* Renderer2D::CreateQuadVertexBuffer()
	{
		// DirectUpload = false: filled through Renderer::Upload (the deferred frame
		// command list) so the write is ordered before the draw within that one list.
		return GraphicsBuffer::CreateVertexBuffer(sizeof(QuadVertex) * m_Params.Capabilities.GetQuadVertexCount(), nullptr, false, "Renderer2DQuadVertexBuffer");
	}

	void Renderer2D::ResetQuadTextureSlots()
	{
		m_TextureSlotIndex = 1; // index 0 is reserved for the white texture
		for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
			m_TextureSlots[i] = nullptr;
	}

	void Renderer2D::FlushQuad()
	{
		if (m_QuadPipeline.IndexCount == 0)
			return;

		// Grow the pool on demand; entries persist and are reused (re-uploaded and
		// re-baked) every frame. BatchIndex is this frame's next free slot.
		if (m_QuadPipeline.BatchIndex >= m_QuadPipeline.VertexBuffers.size())
		{
			m_QuadPipeline.VertexBuffers.push_back(CreateQuadVertexBuffer());
			m_QuadPipeline.RenderPasses.push_back(CreateQuadRenderPass());
		}

		GraphicsBuffer* vertexBuffer = m_QuadPipeline.VertexBuffers[m_QuadPipeline.BatchIndex];
		RenderPass* renderPass = m_QuadPipeline.RenderPasses[m_QuadPipeline.BatchIndex];

		uint32_t dataSize = (uint32_t)((uint8_t*)m_QuadPipeline.VertexBufferPtr - (uint8_t*)m_QuadPipeline.VertexBufferBase);
		Renderer::Upload(vertexBuffer, m_QuadPipeline.VertexBufferBase, dataSize);

		for (uint32_t i = 0; i < m_TextureSlots.size(); i++)
		{
			Texture* texture = m_TextureSlots[i] ? m_TextureSlots[i] : m_TextureSlots[0];
			renderPass->SetTexture(1, texture, i);
		}
		renderPass->Bake();

		Renderer::DrawIndexed(renderPass, vertexBuffer, m_QuadIndexBuffer, m_QuadPipeline.IndexCount);
		++m_Statistics.DrawCalls;

		// Begin a fresh batch.
		m_QuadPipeline.BatchIndex++;
		m_QuadPipeline.IndexCount = 0;
		m_QuadPipeline.VertexBufferPtr = m_QuadPipeline.VertexBufferBase;
		ResetQuadTextureSlots();
	}

	void Renderer2D::DestroyQuadPipeline()
	{
		for (GraphicsBuffer* vertexBuffer : m_QuadPipeline.VertexBuffers)
			vertexBuffer->Destroy();
		m_QuadPipeline.VertexBuffers.clear();

		for (RenderPass* renderPass : m_QuadPipeline.RenderPasses)
			renderPass->Destroy();
		m_QuadPipeline.RenderPasses.clear();

		delete[] m_QuadPipeline.VertexBufferBase;
		m_QuadPipeline.VertexBufferBase = nullptr;
		m_QuadPipeline.VertexBufferPtr = nullptr;
		m_QuadPipeline.IndexCount = 0;
		m_QuadPipeline.BatchIndex = 0;

		if (m_QuadPipeline.Pipeline)
		{
			m_QuadPipeline.Pipeline->Destroy();
			m_QuadPipeline.Pipeline = nullptr;
		}

		if (m_QuadPipeline.Shader)
		{
			m_QuadPipeline.Shader->Destroy();
			m_QuadPipeline.Shader = nullptr;
		}
	}

	/**************************************************
	***		CIRCLE									***
	**************************************************/

	void Renderer2D::CreateCircleRenderPass()
	{
		m_CircleRenderPass = CircleRenderPass();

		m_CircleRenderPass.Shader = Shader::CreateFromSource("Renderer2DCircleShader", Shaders::Renderer2DCircleShader);

		VertexLayout vertexLayout = VertexLayout()
			.SetStride(sizeof(CircleVertex))
			.AddAttribute("a_WorldPosition", Format::RGB32_FLOAT, offsetof(CircleVertex, WorldPosition))
			.AddAttribute("a_LocalPosition", Format::RGB32_FLOAT, offsetof(CircleVertex, LocalPosition))
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(CircleVertex, Color))
			.AddAttribute("a_Thickness", Format::R32_FLOAT, offsetof(CircleVertex, Thickness))
			.AddAttribute("a_Fade", Format::R32_FLOAT, offsetof(CircleVertex, Fade));

		m_CircleRenderPass.Pipeline = Pipeline::Create(PipelineParams()
			.SetDebugName("Renderer2DCirclePipeline")
			.SetFramebuffer(Renderer::GetSwapChainFramebuffer())
			.SetShader(m_CircleRenderPass.Shader)
			.SetVertexLayout(vertexLayout)
			.SetCullMode(CullMode::BackAndFront)
			.SetDepthTest(false)
			.SetDepthWrite(false));

		RenderPassParams renderPassParams = RenderPassParams()
			.SetPipeline(m_CircleRenderPass.Pipeline);

		// Bindings are camera-only and constant, so this single render pass is baked
		// once here and shared by every circle batch.
		m_CircleRenderPass.RenderPass = RenderPass::Create(renderPassParams);
		m_CircleRenderPass.RenderPass->Initialize();
		m_CircleRenderPass.RenderPass->SetUniformBuffer(0, m_CameraUniformBuffer);
		m_CircleRenderPass.RenderPass->Bake();

		m_CircleRenderPass.VertexBufferBase = new CircleVertex[m_Params.Capabilities.GetQuadVertexCount()];
	}

	GraphicsBuffer* Renderer2D::CreateCircleVertexBuffer()
	{
		return GraphicsBuffer::CreateVertexBuffer(sizeof(CircleVertex) * m_Params.Capabilities.GetQuadVertexCount(), nullptr, false, "Renderer2DCircleVertexBuffer");
	}

	void Renderer2D::FlushCircle()
	{
		if (m_CircleRenderPass.IndexCount == 0)
			return;

		if (m_CircleRenderPass.BatchIndex >= m_CircleRenderPass.VertexBuffers.size())
			m_CircleRenderPass.VertexBuffers.push_back(CreateCircleVertexBuffer());

		GraphicsBuffer* vertexBuffer = m_CircleRenderPass.VertexBuffers[m_CircleRenderPass.BatchIndex];

		uint32_t dataSize = (uint32_t)((uint8_t*)m_CircleRenderPass.VertexBufferPtr - (uint8_t*)m_CircleRenderPass.VertexBufferBase);
		Renderer::Upload(vertexBuffer, m_CircleRenderPass.VertexBufferBase, dataSize);

		Renderer::DrawIndexed(m_CircleRenderPass.RenderPass, vertexBuffer, m_QuadIndexBuffer, m_CircleRenderPass.IndexCount);
		++m_Statistics.DrawCalls;

		m_CircleRenderPass.BatchIndex++;
		m_CircleRenderPass.IndexCount = 0;
		m_CircleRenderPass.VertexBufferPtr = m_CircleRenderPass.VertexBufferBase;
	}

	void Renderer2D::DestroyCircleRenderPass()
	{
		for (GraphicsBuffer* vertexBuffer : m_CircleRenderPass.VertexBuffers)
			vertexBuffer->Destroy();
		m_CircleRenderPass.VertexBuffers.clear();

		delete[] m_CircleRenderPass.VertexBufferBase;
		m_CircleRenderPass.VertexBufferBase = nullptr;
		m_CircleRenderPass.VertexBufferPtr = nullptr;
		m_CircleRenderPass.IndexCount = 0;
		m_CircleRenderPass.BatchIndex = 0;

		if (m_CircleRenderPass.Pipeline)
		{
			m_CircleRenderPass.Pipeline->Destroy();
			m_CircleRenderPass.Pipeline = nullptr;
		}

		if (m_CircleRenderPass.Shader)
		{
			m_CircleRenderPass.Shader->Destroy();
			m_CircleRenderPass.Shader = nullptr;
		}

		if (m_CircleRenderPass.RenderPass)
		{
			m_CircleRenderPass.RenderPass->Destroy();
			m_CircleRenderPass.RenderPass = nullptr;
		}
	}

	/**************************************************
	***		TEXT									***
	**************************************************/

	void Renderer2D::CreateTextQuadRenderPass()
	{
		m_TextQuadRenderPass = TextQuadRenderPass();

		m_TextQuadRenderPass.Shader = Shader::CreateFromSource("Renderer2DTextShader", Shaders::Renderer2DTextShader);

		VertexLayout vertexLayout = VertexLayout()
			.SetStride(sizeof(TextVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(TextVertex, Position))
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(TextVertex, Color))
			.AddAttribute("a_TexCoord", Format::RG32_FLOAT, offsetof(TextVertex, TexCoord));

		m_TextQuadRenderPass.Pipeline = Pipeline::Create(PipelineParams()
			.SetDebugName("Renderer2DTextPipeline")
			.SetFramebuffer(Renderer::GetSwapChainFramebuffer())
			.SetShader(m_TextQuadRenderPass.Shader)
			.SetVertexLayout(vertexLayout)
			.SetDepthTest(false)
			.SetDepthWrite(false));

		RenderPassParams renderPassParams = RenderPassParams()
			.SetPipeline(m_TextQuadRenderPass.Pipeline);

		// One shared render pass; the font atlas is bound and the binding set baked
		// per frame (on the first flush) in FlushText.
		m_TextQuadRenderPass.RenderPass = RenderPass::Create(renderPassParams);
		m_TextQuadRenderPass.RenderPass->Initialize();
		m_TextQuadRenderPass.RenderPass->SetUniformBuffer(0, m_CameraUniformBuffer);
		m_TextQuadRenderPass.RenderPass->SetSampler(2, Renderer::GetClampSampler());

		m_TextQuadRenderPass.VertexBufferBase = new TextVertex[m_Params.Capabilities.GetQuadVertexCount()];
	}

	GraphicsBuffer* Renderer2D::CreateTextVertexBuffer()
	{
		return GraphicsBuffer::CreateVertexBuffer(sizeof(TextVertex) * m_Params.Capabilities.GetQuadVertexCount(), nullptr, false, "Renderer2DTextVertexBuffer");
	}

	void Renderer2D::FlushText()
	{
		if (m_TextQuadRenderPass.IndexCount == 0)
			return;

		if (m_TextQuadRenderPass.BatchIndex >= m_TextQuadRenderPass.VertexBuffers.size())
			m_TextQuadRenderPass.VertexBuffers.push_back(CreateTextVertexBuffer());

		GraphicsBuffer* vertexBuffer = m_TextQuadRenderPass.VertexBuffers[m_TextQuadRenderPass.BatchIndex];

		uint32_t dataSize = (uint32_t)((uint8_t*)m_TextQuadRenderPass.VertexBufferPtr - (uint8_t*)m_TextQuadRenderPass.VertexBufferBase);
		Renderer::Upload(vertexBuffer, m_TextQuadRenderPass.VertexBufferBase, dataSize);

		// All text in a frame shares one atlas, so bake the shared render pass once,
		// on the first flush — re-baking it while earlier text draws in this frame
		// still reference its binding set would invalidate them.
		if (m_TextQuadRenderPass.BatchIndex == 0)
		{
			m_TextQuadRenderPass.RenderPass->SetTexture(1, m_FontAtlasTexture);
			m_TextQuadRenderPass.RenderPass->Bake();
		}

		Renderer::DrawIndexed(m_TextQuadRenderPass.RenderPass, vertexBuffer, m_QuadIndexBuffer, m_TextQuadRenderPass.IndexCount);
		++m_Statistics.DrawCalls;

		m_TextQuadRenderPass.BatchIndex++;
		m_TextQuadRenderPass.IndexCount = 0;
		m_TextQuadRenderPass.VertexBufferPtr = m_TextQuadRenderPass.VertexBufferBase;
	}

	void Renderer2D::DestroyTextQuadRenderPass()
	{
		for (GraphicsBuffer* vertexBuffer : m_TextQuadRenderPass.VertexBuffers)
			vertexBuffer->Destroy();
		m_TextQuadRenderPass.VertexBuffers.clear();

		delete[] m_TextQuadRenderPass.VertexBufferBase;
		m_TextQuadRenderPass.VertexBufferBase = nullptr;
		m_TextQuadRenderPass.VertexBufferPtr = nullptr;
		m_TextQuadRenderPass.IndexCount = 0;
		m_TextQuadRenderPass.BatchIndex = 0;

		if (m_TextQuadRenderPass.Pipeline)
		{
			m_TextQuadRenderPass.Pipeline->Destroy();
			m_TextQuadRenderPass.Pipeline = nullptr;
		}

		if (m_TextQuadRenderPass.Shader)
		{
			m_TextQuadRenderPass.Shader->Destroy();
			m_TextQuadRenderPass.Shader = nullptr;
		}

		if (m_TextQuadRenderPass.RenderPass)
		{
			m_TextQuadRenderPass.RenderPass->Destroy();
			m_TextQuadRenderPass.RenderPass = nullptr;
		}
	}


} // namespace Dingo
