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
layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in flat float v_TexIndex;

layout(location = 0) out vec4 o_Color;

layout (set = 0, binding = 1) uniform texture2D u_Textures[32];
layout (set = 0, binding = 2) uniform sampler u_Sampler;

void main()
{
	o_Color = texture(sampler2D(u_Textures[int(v_TexIndex)], u_Sampler), v_TexCoord) * v_Color;
	
	if (o_Color.a == 0.0)
	{
		discard; // Skip rendering if the color is fully transparent
	}
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

	Renderer2D* Renderer2D::Create(Renderer* renderer, const Renderer2DCapabilities& capabilities)
	{
		Renderer2D* renderer2D = new Renderer2D(renderer, Renderer2DParams{
			.Capabilities = capabilities
			});
		renderer2D->Initialize();
		return renderer2D;
	}

	Renderer2D* Renderer2D::Create(Framebuffer* framebuffer, const Renderer2DCapabilities& capabilities)
	{
		Renderer2D* renderer2D = new Renderer2D(Renderer2DParams{
			.TargetFramebuffer = framebuffer,
			.Capabilities = capabilities
			});
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
		if (!m_Renderer)
		{
			m_Renderer = Renderer::Create(m_Params.TargetFramebuffer);
			m_Renderer->Initialize();
		}

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
		CreateTextQuadRenderPass();
	}

	void Renderer2D::Shutdown()
	{
		DestroyTextQuadRenderPass();
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

		if (m_OwnsRenderer && m_Renderer)
		{
			m_Renderer->Shutdown();
			delete m_Renderer;
			m_Renderer = nullptr;
		}
	}

	void Renderer2D::BeginScene(const glm::mat4& projectionViewMatrix)
	{
		m_CameraData.ProjectionViewMatrix = projectionViewMatrix;
		m_CameraUniformBuffer->Upload(&m_CameraData, sizeof(CameraData));

		m_QuadPipeline.IndexCount = 0;
		m_QuadPipeline.VertexBufferPtr = m_QuadPipeline.VertexBufferBase;

		m_TextQuadRenderPass.IndexCount = 0;
		m_TextQuadRenderPass.VertexBufferPtr = m_TextQuadRenderPass.VertexBufferBase;

		m_TextureSlotIndex = 1; // Start from 1 since index 0 is reserved for the white texture
		for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
		{
			m_TextureSlots[i] = nullptr; // Reset texture slots
		}
	}

	void Renderer2D::EndScene()
	{
		m_Renderer->Begin();
		m_Renderer->Clear(m_Params.ClearColor);

		if (m_CameraUniformBuffer)
		{
			m_Renderer->Upload(m_CameraUniformBuffer);
		}

		if (m_QuadPipeline.IndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)m_QuadPipeline.VertexBufferPtr - (uint8_t*)m_QuadPipeline.VertexBufferBase);
			m_QuadPipeline.VertexBuffer->Upload(m_QuadPipeline.VertexBufferBase, dataSize);

			for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
			{
				if (m_TextureSlots[i])
				{
					m_QuadPipeline.RenderPass->SetTexture(1, m_TextureSlots[i], i);
					continue;
				}

				m_QuadPipeline.RenderPass->SetTexture(1, m_TextureSlots[0], i);
			}
			m_QuadPipeline.RenderPass->Bake();

			m_Renderer->BeginRenderPass(m_QuadPipeline.RenderPass);
			m_Renderer->DrawIndexed(m_QuadPipeline.VertexBuffer, m_QuadIndexBuffer, m_QuadPipeline.IndexCount);
			m_Renderer->EndRenderPass();
		}

		if (m_TextQuadRenderPass.IndexCount)
		{
			uint32_t dataSize = (uint32_t)((uint8_t*)m_TextQuadRenderPass.VertexBufferPtr - (uint8_t*)m_TextQuadRenderPass.VertexBufferBase);
			m_TextQuadRenderPass.VertexBuffer->Upload(m_TextQuadRenderPass.VertexBufferBase, dataSize);

			m_TextQuadRenderPass.RenderPass->SetTexture(1, m_FontAtlasTexture);
			m_TextQuadRenderPass.RenderPass->Bake();

			m_Renderer->BeginRenderPass(m_TextQuadRenderPass.RenderPass);
			m_Renderer->DrawIndexed(m_TextQuadRenderPass.VertexBuffer, m_QuadIndexBuffer, m_TextQuadRenderPass.IndexCount);
			m_Renderer->EndRenderPass();
		}

		m_Renderer->End();
	}

	void Renderer2D::Clear(const glm::vec4& clearColor)
	{
		m_Params.ClearColor = clearColor;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad(glm::vec3(position, 0.0f), size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		if (m_QuadPipeline.IndexCount + 6 > m_Params.Capabilities.GetQuadIndexCount())
		{
			DE_CORE_ERROR("Renderer2D: Quad index count exceeded the maximum limit.");
			return;
		}

		constexpr size_t quadVertexCount = 4;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadPipeline.VertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadPipeline.VertexBufferPtr->Color = color;
			m_QuadPipeline.VertexBufferPtr->TexCoord = m_TextureCoords[i];
			m_QuadPipeline.VertexBufferPtr->TexIndex = 0.0f;
			m_QuadPipeline.VertexBufferPtr++;
		}

		m_QuadPipeline.IndexCount += 6;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		DrawQuad(glm::vec3(position, 0.0f), size, texture, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		if (m_QuadPipeline.IndexCount + 6 > m_Params.Capabilities.GetQuadIndexCount())
		{
			DE_CORE_ERROR("Renderer2D: Quad index count exceeded the maximum limit.");
			return;
		}

		float textureIndex = GetTextureIndex(texture);

		constexpr size_t quadVertexCount = 4;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadPipeline.VertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadPipeline.VertexBufferPtr->Color = color;
			m_QuadPipeline.VertexBufferPtr->TexCoord = m_TextureCoords[i];
			m_QuadPipeline.VertexBufferPtr->TexIndex = textureIndex;
			m_QuadPipeline.VertexBufferPtr++;
		}

		m_QuadPipeline.IndexCount += 6;
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

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(size, size, 1.0f));

		m_FontAtlasTexture = fontAtlas;

		double x = 0.0;
		double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
		double y = 0.0f; // -fsScale * metrics.ascenderY;
		//double y = -fsScale * metrics.ascenderY;

		//if(s_Renderer2DData.QuadIndexCount >= Renderer2DData::MaxIndices)
		//{
		//	NextBatch();
		//}

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

			// Render here!
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
		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			if (m_TextureSlots[i]->NativeEquals(texture))
			{
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f)
		{
			//if (m_TextureSlotIndex >= MaxTextureSlots)
			//	FlushAndReset();

			textureIndex = (float)m_TextureSlotIndex;
			m_TextureSlots[m_TextureSlotIndex] = texture;
			m_TextureSlotIndex++;
		}

		return textureIndex;
	}

	void Renderer2D::CreateQuadIndexBuffer()
	{
		uint16_t* quadIndices = new uint16_t[m_Params.Capabilities.GetQuadIndexCount()];

		uint16_t offset = 0;
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

		m_QuadIndexBuffer = GraphicsBuffer::CreateIndexBuffer(sizeof(uint16_t) * m_Params.Capabilities.GetQuadIndexCount(), quadIndices, true, "Renderer2DQuadIndexBuffer");

		delete[] quadIndices;
	}

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
			.SetFramebuffer(m_Renderer->GetTargetFramebuffer())
			.SetShader(m_QuadPipeline.Shader)
			.SetVertexLayout(vertexLayout)
			.SetCullMode(CullMode::BackAndFront));

		m_QuadPipeline.VertexBuffer = GraphicsBuffer::CreateVertexBuffer(sizeof(QuadVertex) * m_Params.Capabilities.GetQuadVertexCount(), nullptr, true, "Renderer2DQuadVertexBuffer");

		RenderPassParams renderPassParams = RenderPassParams()
			.SetPipeline(m_QuadPipeline.Pipeline);

		m_QuadPipeline.RenderPass = RenderPass::Create(renderPassParams);
		m_QuadPipeline.RenderPass->Initialize();
		m_QuadPipeline.RenderPass->SetUniformBuffer(0, m_CameraUniformBuffer);
		m_QuadPipeline.RenderPass->SetSampler(2, Renderer::GetClampSampler());
		for (uint32_t i = 0; i < MaxTextureSlots; i++)
		{
			m_QuadPipeline.RenderPass->SetTexture(1, Renderer::GetWhiteTexture(), i);
		}
		m_QuadPipeline.RenderPass->Bake();

		m_QuadPipeline.VertexBufferBase = new QuadVertex[m_Params.Capabilities.GetQuadVertexCount()];
	}

	void Renderer2D::DestroyQuadPipeline()
	{
		if (m_QuadPipeline.VertexBuffer)
		{
			m_QuadPipeline.VertexBuffer->Destroy();
			delete[] m_QuadPipeline.VertexBufferBase;
			m_QuadPipeline.VertexBuffer = nullptr;
			m_QuadPipeline.VertexBufferBase = nullptr;
			m_QuadPipeline.VertexBufferPtr = nullptr;
		}
		m_QuadPipeline.IndexCount = 0;

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

		if (m_QuadPipeline.RenderPass)
		{
			m_QuadPipeline.RenderPass->Destroy();
			m_QuadPipeline.RenderPass = nullptr;
		}
	}

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
			.SetFramebuffer(m_Renderer->GetTargetFramebuffer())
			.SetShader(m_TextQuadRenderPass.Shader)
			.SetVertexLayout(vertexLayout));

		m_TextQuadRenderPass.VertexBuffer = GraphicsBuffer::CreateVertexBuffer(sizeof(TextVertex) * m_Params.Capabilities.GetQuadVertexCount(), nullptr, true, "Renderer2DTextVertexBuffer");

		RenderPassParams renderPassParams = RenderPassParams()
			.SetPipeline(m_TextQuadRenderPass.Pipeline);

		m_TextQuadRenderPass.RenderPass = RenderPass::Create(renderPassParams);
		m_TextQuadRenderPass.RenderPass->Initialize();
		m_TextQuadRenderPass.RenderPass->SetUniformBuffer(0, m_CameraUniformBuffer);
		m_TextQuadRenderPass.RenderPass->SetSampler(2, Renderer::GetClampSampler());
		m_TextQuadRenderPass.RenderPass->Bake();

		m_TextQuadRenderPass.VertexBufferBase = new TextVertex[m_Params.Capabilities.GetQuadVertexCount()];
	}

	void Renderer2D::DestroyTextQuadRenderPass()
	{
		if (m_TextQuadRenderPass.VertexBuffer)
		{
			m_TextQuadRenderPass.VertexBuffer->Destroy();
			delete[] m_TextQuadRenderPass.VertexBufferBase;
			m_TextQuadRenderPass.VertexBuffer = nullptr;
			m_TextQuadRenderPass.VertexBufferBase = nullptr;
			m_TextQuadRenderPass.VertexBufferPtr = nullptr;
		}
		m_TextQuadRenderPass.IndexCount = 0;

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
