#include "depch.h"
#include "DingoUIDrawList.h"

#include "DingoEngine/Graphics/Font.h"
#include "DingoEngine/Graphics/MSDFData.h"
#include "DingoEngine/Graphics/TextLayout.h"
#include "DingoEngine/Graphics/Texture.h"

namespace Dingo::Internal
{

	namespace
	{
		// The distance range Font bakes its atlases with.
		constexpr float k_MsdfPixelRange = 2.0f;

		struct Hasher
		{
			uint64_t Value;

			void Bytes(const void* data, size_t size)
			{
				const uint8_t* bytes = static_cast<const uint8_t*>(data);
				for (size_t i = 0; i < size; ++i)
				{
					Value ^= bytes[i];
					Value *= 1099511628211ull;
				}
			}

			template<typename T>
			void Add(const T& value) { Bytes(&value, sizeof(T)); }

			void Add(const glm::vec2& value) { Add(value.x); Add(value.y); }
			void Add(const glm::vec4& value) { Add(value.x); Add(value.y); Add(value.z); Add(value.w); }
			void Add(const UIRect& rect) { Add(rect.Min); Add(rect.Max); }
		};

		struct QuadCorners
		{
			std::array<glm::vec2, 4> TexCoords; // top-left, top-right, bottom-right, bottom-left
		};

		class GeometryBuilder
		{
		public:
			explicit GeometryBuilder(UIGeometry& geometry)
				: m_Geometry(geometry)
			{
				m_Geometry.Clear();
				NewBatch();
			}

			~GeometryBuilder()
			{
				if (m_Geometry.Batches.back().IndexCount == 0)
					m_Geometry.Batches.pop_back();
			}

			void Shape(const UIRect& rect, const QuadCorners& corners, Texture* texture, const glm::vec4& color, const glm::vec4& borderColor, const glm::vec4& radii, float borderWidth, const UIRect& clip)
			{
				const glm::vec2 half = (rect.Max - rect.Min) * 0.5f;
				const float largest = (std::min)(half.x, half.y);
				const glm::vec4 clampedRadii = glm::clamp(radii, glm::vec4(0.0f), glm::vec4(largest));
				const float border = glm::clamp(borderWidth, 0.0f, largest);

				const float slot = static_cast<float>(SlotFor(texture));
				const std::array<glm::vec2, 4> offsets = { glm::vec2(-half.x, -half.y), glm::vec2(half.x, -half.y), glm::vec2(half.x, half.y), glm::vec2(-half.x, half.y) };
				const std::array<glm::vec2, 4> positions = { rect.Min, glm::vec2(rect.Max.x, rect.Min.y), rect.Max, glm::vec2(rect.Min.x, rect.Max.y) };
				for (size_t i = 0; i < 4; ++i)
				{
					UIVertex& vertex = m_Geometry.Vertices.emplace_back();
					vertex.Position = positions[i];
					vertex.TexCoord = corners.TexCoords[i];
					vertex.Color = color;
					vertex.BorderColor = borderColor;
					vertex.Local = glm::vec4(offsets[i], half);
					vertex.Radii = clampedRadii;
					vertex.Clip = glm::vec4(clip.Min, clip.Max);
					vertex.Params = glm::vec4(border, slot, static_cast<float>(UIQuadKind::Shape), 0.0f);
				}
				EndQuad();
			}

			void Glyph(const UIRect& rect, const QuadCorners& corners, Texture* atlas, const glm::vec4& color, float screenPixelRange, const UIRect& clip)
			{
				const float slot = static_cast<float>(SlotFor(atlas));
				const std::array<glm::vec2, 4> positions = { rect.Min, glm::vec2(rect.Max.x, rect.Min.y), rect.Max, glm::vec2(rect.Min.x, rect.Max.y) };
				for (size_t i = 0; i < 4; ++i)
				{
					UIVertex& vertex = m_Geometry.Vertices.emplace_back();
					vertex.Position = positions[i];
					vertex.TexCoord = corners.TexCoords[i];
					vertex.Color = color;
					vertex.Clip = glm::vec4(clip.Min, clip.Max);
					vertex.Params = glm::vec4(0.0f, slot, static_cast<float>(UIQuadKind::Glyph), screenPixelRange);
				}
				EndQuad();
			}

			void Cull() { ++m_Geometry.CulledQuads; }

		private:
			void NewBatch()
			{
				m_Geometry.Batches.emplace_back().FirstIndex = m_Geometry.QuadCount * 6;
			}

			uint32_t SlotFor(Texture* texture)
			{
				if (!texture)
					return 0;

				UIDrawBatch* batch = &m_Geometry.Batches.back();
				for (uint32_t i = 1; i < batch->TextureCount; ++i)
				{
					if (batch->Textures[i] == texture)
						return i;
				}

				if (batch->TextureCount == k_UITextureSlots)
				{
					NewBatch();
					batch = &m_Geometry.Batches.back();
				}
				batch->Textures[batch->TextureCount] = texture;
				return batch->TextureCount++;
			}

			void EndQuad()
			{
				m_Geometry.Batches.back().IndexCount += 6;
				++m_Geometry.QuadCount;
			}

			UIGeometry& m_Geometry;
		};

		QuadCorners ImageCorners(const glm::vec2& uvMin, const glm::vec2& uvMax)
		{
			return { { glm::vec2(uvMin.x, uvMax.y), uvMax, glm::vec2(uvMax.x, uvMin.y), uvMin } };
		}

		bool IsInvisible(const glm::vec4& fill, float borderWidth, const glm::vec4& borderColor)
		{
			return fill.a <= 0.0f && (borderWidth <= 0.0f || borderColor.a <= 0.0f);
		}
	}

	void UIDrawList::AddRect(const UIRect& rect, const glm::vec4& fill, float radius, float borderWidth, const glm::vec4& borderColor, const UIRect& clip)
	{
		AddRect(rect, fill, glm::vec4(radius), borderWidth, borderColor, clip);
	}

	void UIDrawList::AddRect(const UIRect& rect, const glm::vec4& fill, const glm::vec4& radii, float borderWidth, const glm::vec4& borderColor, const UIRect& clip)
	{
		Command& command = m_Commands.emplace_back();
		command.Kind = CommandKind::Rect;
		command.Rect = rect;
		command.Clip = clip;
		command.Color = fill;
		command.BorderColor = borderColor;
		command.Radii = radii;
		command.BorderWidth = borderWidth;
	}

	void UIDrawList::AddImage(const UIRect& rect, Texture* texture, const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec4& tint, const UIRect& clip)
	{
		Command& command = m_Commands.emplace_back();
		command.Kind = CommandKind::Image;
		command.Rect = rect;
		command.Clip = clip;
		command.Color = tint;
		command.UVMin = uvMin;
		command.UVMax = uvMax;
		command.Image = texture;
	}

	void UIDrawList::AddText(const glm::vec2& position, const Font* font, std::string_view text, float size, const glm::vec4& color, float maxWidth, const UIRect& clip, float align, float alignWidth)
	{
		if (!font || text.empty() || size <= 0.0f)
			return;

		Command& command = m_Commands.emplace_back();
		command.Kind = CommandKind::Text;
		command.Rect = { position, position };
		command.Clip = clip;
		command.Color = color;
		command.TextSize = size;
		command.MaxWidth = maxWidth;
		command.Align = align;
		command.AlignWidth = alignWidth;
		command.TextFont = font;
		command.TextOffset = static_cast<uint32_t>(m_Text.size());
		command.TextLength = static_cast<uint32_t>(text.size());
		m_Text.append(text);
	}

	void UIDrawList::Clear()
	{
		m_Commands.clear();
		m_Text.clear();
	}

	uint64_t UIDrawList::Hash(uint64_t seed) const
	{
		Hasher hasher{ 14695981039346656037ull ^ seed };
		for (const Command& command : m_Commands)
		{
			hasher.Add(command.Kind);
			hasher.Add(command.Rect);
			hasher.Add(command.Clip);
			hasher.Add(command.Color);
			switch (command.Kind)
			{
			case CommandKind::Rect:
				hasher.Add(command.BorderColor);
				hasher.Add(command.Radii);
				hasher.Add(command.BorderWidth);
				break;
			case CommandKind::Image:
				hasher.Add(command.UVMin);
				hasher.Add(command.UVMax);
				hasher.Add(command.Image ? command.Image->GetId() : 0ull);
				break;
			case CommandKind::Text:
			{
				const Texture* atlas = command.TextFont->GetAtlasTexture();
				hasher.Add(atlas ? atlas->GetId() : 0ull);
				hasher.Add(command.TextSize);
				hasher.Add(command.MaxWidth);
				hasher.Add(command.Align);
				hasher.Add(command.AlignWidth);
				hasher.Add(command.TextLength);
				hasher.Bytes(m_Text.data() + command.TextOffset, command.TextLength);
				break;
			}
			}
		}
		return hasher.Value;
	}

	void UIDrawList::Build(UIGeometry& geometry) const
	{
		DE_PROFILE_FUNCTION();

		GeometryBuilder builder(geometry);
		for (const Command& command : m_Commands)
		{
			switch (command.Kind)
			{
			case CommandKind::Rect:
				if (command.Rect.IsEmpty() || !command.Rect.Overlaps(command.Clip) || IsInvisible(command.Color, command.BorderWidth, command.BorderColor))
				{
					builder.Cull();
					break;
				}
				builder.Shape(command.Rect, ImageCorners(glm::vec2(0.0f), glm::vec2(1.0f)), nullptr, command.Color, command.BorderColor, command.Radii, command.BorderWidth, command.Clip);
				break;

			case CommandKind::Image:
				if (command.Rect.IsEmpty() || !command.Rect.Overlaps(command.Clip) || command.Color.a <= 0.0f)
				{
					builder.Cull();
					break;
				}
				builder.Shape(command.Rect, ImageCorners(command.UVMin, command.UVMax), command.Image, command.Color, glm::vec4(0.0f), glm::vec4(0.0f), 0.0f, command.Clip);
				break;

			case CommandKind::Text:
			{
				const TextLayout::FontGlyphSource source(*command.TextFont);
				Texture* atlas = command.TextFont->GetAtlasTexture();
				if (!source.Geometry || !atlas || command.Color.a <= 0.0f)
					break;

				const float size = command.TextSize;
				const glm::vec2 texel = 1.0f / glm::vec2(static_cast<float>(atlas->GetWidth()), static_cast<float>(atlas->GetHeight()));
				const glm::vec2 origin = command.Rect.Min;
				const float firstBaseline = origin.y + static_cast<float>(source.Ascender) * size;
				const float lineAdvance = static_cast<float>(source.LineHeight) * size;
				const double scale = source.Scale;

				const std::string_view text(m_Text.data() + command.TextOffset, command.TextLength);
				const TextLayout::Params params{ .Size = size, .MaxWidth = command.MaxWidth };
				std::vector<float> lineOffsets;
				if (command.Align > 0.0f && command.AlignWidth > 0.0f)
				{
					TextLayout::Walk(source, text, params,
						[](TextLayout::FontGlyphSource::Glyph, uint32_t, double, uint32_t) {},
						[&](uint32_t, double width)
						{
							lineOffsets.push_back((std::max)(0.0f, command.AlignWidth - static_cast<float>(width) * size) * command.Align);
						});
				}
				TextLayout::Walk(source, text, params,
					[&](TextLayout::FontGlyphSource::Glyph glyph, uint32_t, double penX, uint32_t line)
					{
						double planeLeft, planeBottom, planeRight, planeTop;
						glyph->getQuadPlaneBounds(planeLeft, planeBottom, planeRight, planeTop);
						double atlasLeft, atlasBottom, atlasRight, atlasTop;
						glyph->getQuadAtlasBounds(atlasLeft, atlasBottom, atlasRight, atlasTop);
						if (atlasRight <= atlasLeft)
							return;

						const float baseline = firstBaseline + static_cast<float>(line) * lineAdvance;
						const float originX = origin.x + (line < lineOffsets.size() ? lineOffsets[line] : 0.0f);
						UIRect quad;
						quad.Min = { originX + static_cast<float>((penX + planeLeft * scale) * size), baseline - static_cast<float>(planeTop * scale * size) };
						quad.Max = { originX + static_cast<float>((penX + planeRight * scale) * size), baseline - static_cast<float>(planeBottom * scale * size) };
						if (quad.IsEmpty() || !quad.Overlaps(command.Clip))
						{
							builder.Cull();
							return;
						}

						const glm::vec2 uvMin = glm::vec2(static_cast<float>(atlasLeft), static_cast<float>(atlasBottom)) * texel;
						const glm::vec2 uvMax = glm::vec2(static_cast<float>(atlasRight), static_cast<float>(atlasTop)) * texel;
						const float screenPixelRange = (std::max)(k_MsdfPixelRange * (quad.Max.x - quad.Min.x) / static_cast<float>(atlasRight - atlasLeft), 1.0f);
						builder.Glyph(quad, ImageCorners(uvMin, uvMax), atlas, command.Color, screenPixelRange, command.Clip);
					},
					[](uint32_t, double) {});
				break;
			}
			}
		}
	}

}
