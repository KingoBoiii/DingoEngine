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
			if (rotation == 0.0f)
			{
				// translate * scale written out — the general path builds three matrices and
				// multiplies them for what is a diagonal plus a translation column.
				glm::mat4 transform(1.0f);
				transform[0][0] = size.x;
				transform[1][1] = size.y;
				transform[3] = glm::vec4(position, 1.0f);
				return transform;
			}

			glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
			transform *= glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0.0f, 0.0f, 1.0f });
			transform *= glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

			return transform;
		}

		// An unrotated quad corner is just position + size * corner, so the axis-aligned
		// DrawQuad paths need neither the 4x4 nor a mat4 * vec4 per vertex.
		inline static glm::vec3 QuadCorner(const glm::vec3& position, const glm::vec2& size, const glm::vec4& corner)
		{
			return glm::vec3(position.x + size.x * corner.x, position.y + size.y * corner.y, position.z);
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

		m_TextureSlots[0] = Renderer::GetWhiteTexture();
		for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
		{
			m_TextureSlots[i] = nullptr;
		}

		CreateQuadPass();
		CreateCirclePass();
		CreateTextPass();
	}

	void Renderer2D::Shutdown()
	{
		m_TextPass.Destroy();
		m_CirclePass.Destroy();
		m_QuadPass.Destroy();

		DestroyAndDelete(m_QuadIndexBuffer);
		DestroyAndDelete(m_CameraUniformBuffer);
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

		m_QuadPass.Reset();
		m_CirclePass.Reset();
		m_TextPass.Reset();

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
		if (!m_QuadPass.HasRoomForQuad())
			FlushQuad();

		constexpr size_t quadVertexCount = 4;

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadPass.VertexBufferPtr->Position = Utils::QuadCorner(position, size, m_QuadVertexPositions[i]);
			m_QuadPass.VertexBufferPtr->Color = color;
			m_QuadPass.VertexBufferPtr->TexCoord = m_TextureCoords[i];
			m_QuadPass.VertexBufferPtr->TexIndex = 0.0f;
			m_QuadPass.VertexBufferPtr++;
		}

		m_QuadPass.IndexCount += 6;
		++m_Statistics.QuadCount;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		DrawQuad(glm::vec3(position, 0.0f), size, texture, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		if (!m_QuadPass.HasRoomForQuad())
			FlushQuad();

		float textureIndex = GetTextureIndex(texture);

		constexpr size_t quadVertexCount = 4;

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadPass.VertexBufferPtr->Position = Utils::QuadCorner(position, size, m_QuadVertexPositions[i]);
			m_QuadPass.VertexBufferPtr->Color = color;
			m_QuadPass.VertexBufferPtr->TexCoord = m_TextureCoords[i];
			m_QuadPass.VertexBufferPtr->TexIndex = textureIndex;
			m_QuadPass.VertexBufferPtr++;
		}

		m_QuadPass.IndexCount += 6;
		++m_Statistics.QuadCount;
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec2& position, float rotation, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		DrawRotatedQuad(glm::vec3(position, 0.0f), rotation, size, texture, color);
	}

	void Renderer2D::DrawRotatedQuad(const glm::vec3& position, float rotation, const glm::vec2& size, Texture* texture, const glm::vec4& color)
	{
		if (!m_QuadPass.HasRoomForQuad())
			FlushQuad();

		float textureIndex = GetTextureIndex(texture);

		constexpr size_t quadVertexCount = 4;

		glm::mat4 transform = Utils::CreateTransform(position, size, rotation);

		for (size_t i = 0; i < quadVertexCount; i++)
		{
			m_QuadPass.VertexBufferPtr->Position = transform * m_QuadVertexPositions[i];
			m_QuadPass.VertexBufferPtr->Color = color;
			m_QuadPass.VertexBufferPtr->TexCoord = m_TextureCoords[i];
			m_QuadPass.VertexBufferPtr->TexIndex = textureIndex;
			m_QuadPass.VertexBufferPtr++;
		}

		m_QuadPass.IndexCount += 6;
		++m_Statistics.QuadCount;
	}

	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade)
	{
		if (!m_CirclePass.HasRoomForQuad())
			FlushCircle();

		for (size_t i = 0; i < 4; i++)
		{
			m_CirclePass.VertexBufferPtr->WorldPosition = transform * m_QuadVertexPositions[i];
			m_CirclePass.VertexBufferPtr->LocalPosition = m_QuadVertexPositions[i] * 2.0f;
			m_CirclePass.VertexBufferPtr->Color = color;
			m_CirclePass.VertexBufferPtr->Thickness = thickness;
			m_CirclePass.VertexBufferPtr->Fade = fade;
			m_CirclePass.VertexBufferPtr++;
		}

		m_CirclePass.IndexCount += 6;
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

		// A batch samples one atlas, and the glyph UVs are atlas-relative: switching font
		// mid-batch would draw the earlier string's glyphs out of this font's atlas.
		if (m_FontAtlasTexture && m_FontAtlasTexture != fontAtlas)
			FlushText();

		m_FontAtlasTexture = fontAtlas;

		// Centering shifts the emitted quads once the width is known, so they all have to
		// still be in the batch when the string ends: reserve the room up front (one quad
		// per character is a safe over-estimate). A string too long for an empty batch
		// cannot avoid a mid-string flush, so that case pays for a measuring walk instead.
		glm::vec3 origin = position;
		bool shiftAfterEmit = false;
		if (textParameters.Centered)
		{
			if (!m_TextPass.HasRoomForQuads(string.size()))
				FlushText();

			shiftAfterEmit = m_TextPass.HasRoomForQuads(string.size());
			if (!shiftAfterEmit)
				origin.x -= font->GetStringWidth(string, size, textParameters.Kerning) * 0.5f;
		}

		glm::mat4 transform = Utils::CreateTransform(origin, glm::vec2(size, size));

		TextVertex* const firstVertex = m_TextPass.VertexBufferPtr;
		double widestLine = 0.0;

		double x = 0.0;
		double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
		double y = 0.0f; // -fsScale * metrics.ascenderY;
		//double y = -fsScale * metrics.ascenderY;

		// A charset without a space is degenerate but must not crash: GetStringWidth guards
		// the same lookup, and the two have to stay measurable against each other.
		auto spaceGlyph = fontGeometry.getGlyph(' ');
		float spaceGlyphAdvance = spaceGlyph ? (float)spaceGlyph->getAdvance() : 0.0f;

		for (size_t i = 0; i < string.size(); i++)
		{
			char character = string[i];

			if (character == '\n')
			{
				if (x > widestLine)
					widestLine = x;

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
				break;
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

			if (!m_TextPass.HasRoomForQuad())
				FlushText();

			m_TextPass.VertexBufferPtr->Position = transform * glm::vec4(quadMin, 0.0f, 1.0f);
			m_TextPass.VertexBufferPtr->Color = textParameters.Color;
			m_TextPass.VertexBufferPtr->TexCoord = texCoordMin;
			m_TextPass.VertexBufferPtr++;

			m_TextPass.VertexBufferPtr->Position = transform * glm::vec4(quadMin.x, quadMax.y, 0.0f, 1.0f);
			m_TextPass.VertexBufferPtr->Color = textParameters.Color;
			m_TextPass.VertexBufferPtr->TexCoord = { texCoordMin.x, texCoordMax.y };
			m_TextPass.VertexBufferPtr++;

			m_TextPass.VertexBufferPtr->Position = transform * glm::vec4(quadMax, 0.0f, 1.0f);
			m_TextPass.VertexBufferPtr->Color = textParameters.Color;
			m_TextPass.VertexBufferPtr->TexCoord = texCoordMax;
			m_TextPass.VertexBufferPtr++;

			m_TextPass.VertexBufferPtr->Position = transform * glm::vec4(quadMax.x, quadMin.y, 0.0f, 1.0f);
			m_TextPass.VertexBufferPtr->Color = textParameters.Color;
			m_TextPass.VertexBufferPtr->TexCoord = { texCoordMax.x, texCoordMin.y };
			m_TextPass.VertexBufferPtr++;

			m_TextPass.IndexCount += 6;
			++m_Statistics.TextQuadCount;

			// Advance past the last glyph too, so the pen ends on the line's full width —
			// what GetStringWidth reports, and what centering below has to agree with.
			double advance = glyph->getAdvance();
			if (i < string.size() - 1)
				fontGeometry.getAdvance(advance, character, string[i + 1]);

			x += fsScale * advance + textParameters.Kerning;
		}

		if (x > widestLine)
			widestLine = x;

		if (shiftAfterEmit)
		{
			// The text transform is translate + scale only, so centering is a plain offset
			// on the world-space x of every quad this string emitted.
			const float offsetX = -static_cast<float>(widestLine) * size * 0.5f;
			for (TextVertex* vertex = firstVertex; vertex != m_TextPass.VertexBufferPtr; ++vertex)
				vertex->Position.x += offsetX;
		}
	}

	float Renderer2D::GetTextureIndex(Texture* texture)
	{
		// A null texture must never land in m_TextureSlots: every occupied slot is
		// deref'd unchecked below on the next call. Route it to the white texture instead.
		if (!texture)
			return 0.0f;

		for (uint32_t i = 1; i < m_TextureSlotIndex; i++)
		{
			// Pointer identity first: games reuse Texture* objects, so this settles nearly
			// every lookup without NativeEquals, which is a virtual call wrapping a
			// dynamic_cast and runs once per occupied slot for every textured quad.
			if (m_TextureSlots[i] == texture || m_TextureSlots[i]->NativeEquals(texture))
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

	void Renderer2D::CreateQuadPass()
	{
		VertexLayout vertexLayout = VertexLayout()
			.SetStride(sizeof(QuadVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(QuadVertex, Position))
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(QuadVertex, Color))
			.AddAttribute("a_TexCoord", Format::RG32_FLOAT, offsetof(QuadVertex, TexCoord))
			.AddAttribute("a_TexIndex", Format::R32_FLOAT, offsetof(QuadVertex, TexIndex));

		m_QuadPass.Initialize(BatchPassParams{
			.Name = "Renderer2DQuad",
			.ShaderSource = Shaders::Renderer2DQuadShader,
			.VertexLayout = vertexLayout,
			.CullMode = CullMode::BackAndFront,
			.Capabilities = m_Params.Capabilities,
			.CameraUniformBuffer = m_CameraUniformBuffer,
			.IndexBuffer = m_QuadIndexBuffer,
			.BatchSampler = Renderer::GetPointSampler() });
	}

	void Renderer2D::ResetQuadTextureSlots()
	{
		m_TextureSlotIndex = 1; // index 0 is reserved for the white texture
		for (uint32_t i = 1; i < m_TextureSlots.size(); i++)
			m_TextureSlots[i] = nullptr;
	}

	void Renderer2D::FlushQuad()
	{
		const bool flushed = m_QuadPass.Flush([this](RenderPass* renderPass)
		{
			// The shader samples a fixed 32-element array, so every slot must resolve to
			// a real texture — the ones this batch never claimed included.
			for (uint32_t i = 0; i < m_TextureSlots.size(); i++)
			{
				Texture* texture = m_TextureSlots[i] ? m_TextureSlots[i] : m_TextureSlots[0];
				renderPass->SetTexture(k_TextureBinding, texture, i);
			}
		});

		if (!flushed)
			return;

		++m_Statistics.DrawCalls;
		ResetQuadTextureSlots();
	}

	/**************************************************
	***		CIRCLE									***
	**************************************************/

	void Renderer2D::CreateCirclePass()
	{
		VertexLayout vertexLayout = VertexLayout()
			.SetStride(sizeof(CircleVertex))
			.AddAttribute("a_WorldPosition", Format::RGB32_FLOAT, offsetof(CircleVertex, WorldPosition))
			.AddAttribute("a_LocalPosition", Format::RGB32_FLOAT, offsetof(CircleVertex, LocalPosition))
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(CircleVertex, Color))
			.AddAttribute("a_Thickness", Format::R32_FLOAT, offsetof(CircleVertex, Thickness))
			.AddAttribute("a_Fade", Format::R32_FLOAT, offsetof(CircleVertex, Fade));

		m_CirclePass.Initialize(BatchPassParams{
			.Name = "Renderer2DCircle",
			.ShaderSource = Shaders::Renderer2DCircleShader,
			.VertexLayout = vertexLayout,
			.CullMode = CullMode::BackAndFront,
			.Capabilities = m_Params.Capabilities,
			.CameraUniformBuffer = m_CameraUniformBuffer,
			.IndexBuffer = m_QuadIndexBuffer });
	}

	void Renderer2D::FlushCircle()
	{
		if (m_CirclePass.Flush())
			++m_Statistics.DrawCalls;
	}

	/**************************************************
	***		TEXT									***
	**************************************************/

	void Renderer2D::CreateTextPass()
	{
		VertexLayout vertexLayout = VertexLayout()
			.SetStride(sizeof(TextVertex))
			.AddAttribute("a_Position", Format::RGB32_FLOAT, offsetof(TextVertex, Position))
			.AddAttribute("a_Color", Format::RGBA32_FLOAT, offsetof(TextVertex, Color))
			.AddAttribute("a_TexCoord", Format::RG32_FLOAT, offsetof(TextVertex, TexCoord));

		m_TextPass.Initialize(BatchPassParams{
			.Name = "Renderer2DText",
			.ShaderSource = Shaders::Renderer2DTextShader,
			.VertexLayout = vertexLayout,
			.Capabilities = m_Params.Capabilities,
			.CameraUniformBuffer = m_CameraUniformBuffer,
			.IndexBuffer = m_QuadIndexBuffer,
			.BatchSampler = Renderer::GetClampSampler() });
	}

	void Renderer2D::FlushText()
	{
		const bool flushed = m_TextPass.Flush([this](RenderPass* renderPass)
		{
			renderPass->SetTexture(k_TextureBinding, m_FontAtlasTexture);
		});

		if (flushed)
			++m_Statistics.DrawCalls;
	}


} // namespace Dingo
