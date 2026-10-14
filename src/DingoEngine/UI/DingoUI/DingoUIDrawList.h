#pragma once

#include <glm/glm.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{
	class Texture;
	class Font;
}

namespace Dingo::Internal
{

	struct UIRect
	{
		glm::vec2 Min{ 0.0f };
		glm::vec2 Max{ 0.0f };

		bool Contains(const glm::vec2& point) const
		{
			return point.x >= Min.x && point.y >= Min.y && point.x < Max.x && point.y < Max.y;
		}

		bool IsEmpty() const { return Max.x <= Min.x || Max.y <= Min.y; }

		bool Overlaps(const UIRect& other) const
		{
			return Min.x < other.Max.x && other.Min.x < Max.x && Min.y < other.Max.y && other.Min.y < Max.y;
		}
	};

	// Far enough out to never clip, near enough that the shader compares it exactly.
	inline constexpr float k_UIUnbounded = 1.0e6f;
	inline constexpr UIRect k_UINoClip{ { -k_UIUnbounded, -k_UIUnbounded }, { k_UIUnbounded, k_UIUnbounded } };

	// Slot 0 of every draw is the white texture, so a solid shape samples it like an image.
	inline constexpr uint32_t k_UITextureSlots = 32;

	enum class UIQuadKind : uint32_t
	{
		Shape = 0, // rounded rect with border, textured (white for a solid fill)
		Glyph = 1, // MSDF glyph from a font atlas
		ShapePoint = 2 // as Shape, sampling the texture with the point sampler
	};

	// Mirrors DingoUI.glsl's inputs, in location order. Pixels of the target, origin top-left, +Y down.
	struct UIVertex
	{
		glm::vec2 Position{ 0.0f };
		glm::vec2 TexCoord{ 0.0f };
		glm::vec4 Color{ 1.0f };        // fill, tint or text colour, straight alpha
		glm::vec4 BorderColor{ 0.0f };
		glm::vec4 Local{ 0.0f };        // xy: offset from the quad's centre, zw: half its size
		glm::vec4 Radii{ 0.0f };        // corners: top-left, top-right, bottom-right, bottom-left
		glm::vec4 Clip{ 0.0f };         // min.xy, max.xy
		glm::vec4 Params{ 0.0f };       // x: border width, y: texture slot, z: UIQuadKind, w: MSDF screen pixel range
	};

	struct UIDrawBatch
	{
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
		// [0] is always null (the white texture); TextureCount includes it.
		std::array<Texture*, k_UITextureSlots> Textures{};
		uint32_t TextureCount = 1;
	};

	struct UIGeometry
	{
		std::vector<UIVertex> Vertices; // four a quad: top-left, top-right, bottom-right, bottom-left
		std::vector<UIDrawBatch> Batches;
		uint32_t QuadCount = 0;
		uint32_t CulledQuads = 0;

		void Clear()
		{
			Vertices.clear();
			Batches.clear();
			QuadCount = 0;
			CulledQuads = 0;
		}
	};

	struct UIRenderStatistics
	{
		uint32_t DrawCalls = 0;
		uint32_t Vertices = 0;
		uint32_t Quads = 0;
		uint32_t CulledQuads = 0;  // outside their clip rect, or invisible
		uint32_t CacheHits = 0;    // 1 when the frame drew last frame's vertices again
		uint32_t Uploads = 0;      // 1 when the frame rebuilt and uploaded them
		uint64_t UploadBytes = 0;
		float BuildMs = 0.0f;      // CPU time turning the draw list into vertices
		float UploadMs = 0.0f;     // CPU time uploading them and baking the binding sets
		float DrawMs = 0.0f;       // CPU time recording the draws
	};

	class UICpuTimer
	{
	public:
		UICpuTimer() : m_Start(std::chrono::steady_clock::now()) {}
		float ElapsedMs() const { return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - m_Start).count(); }

	private:
		std::chrono::steady_clock::time_point m_Start;
	};

	// One frame of game UI as drawing commands, in painter's order. Each command keeps its own clip
	// rect, which the shader applies, so clipping never splits a draw.
	class UIDrawList
	{
	public:
		// radius rounds every corner. borderWidth is inset from rect's edge.
		void AddRect(const UIRect& rect, const glm::vec4& fill, float radius = 0.0f, float borderWidth = 0.0f, const glm::vec4& borderColor = glm::vec4(0.0f), const UIRect& clip = k_UINoClip);
		// radii: top-left, top-right, bottom-right, bottom-left.
		void AddRect(const UIRect& rect, const glm::vec4& fill, const glm::vec4& radii, float borderWidth, const glm::vec4& borderColor, const UIRect& clip = k_UINoClip);
		// UVs as Renderer2D's: (0, 0) at the rect's bottom-left, so an image CreateFromFile loaded
		// stands upright. A null texture draws the tint alone.
		void AddImage(const UIRect& rect, Texture* texture, const glm::vec2& uvMin = glm::vec2(0.0f), const glm::vec2& uvMax = glm::vec2(1.0f), const glm::vec4& tint = glm::vec4(1.0f), const UIRect& clip = k_UINoClip);
		// Nine quads (eight without the centre) from one texture of textureSize pixels. borders (left, top, right, bottom)
		// are in texture pixels; each is drawn borders * scale pixels wide, rounded, and shrunk
		// together when two opposite ones would not fit rect. Corners keep their size, edges stretch
		// along one axis, the centre along both. UVs as AddImage's.
		void AddNineSlice(const UIRect& rect, Texture* texture, const glm::vec2& textureSize, const glm::vec4& borders, float scale = 1.0f, bool fillCenter = true, bool pointFilter = false, const glm::vec4& tint = glm::vec4(1.0f), const UIRect& clip = k_UINoClip);
		// position is the top-left of the first line's box; size is the ascender-to-descender
		// height in pixels, as Font::MeasureText measures it. maxWidth > 0 wraps. align (0 left,
		// 0.5 centre, 1 right) places each line within alignWidth pixels from position.
		void AddText(const glm::vec2& position, const Font* font, std::string_view text, float size, const glm::vec4& color, float maxWidth = 0.0f, const UIRect& clip = k_UINoClip, float align = 0.0f, float alignWidth = 0.0f);

		// Appends `other`'s commands as they are.
		void Append(const UIDrawList& other);
		// Appends `other`'s commands moved, scaled about `pivot` and faded: p' = pivot + (p - pivot) * scale + offset,
		// sizes, radii, borders and 9-slice scales times `scale`, alpha times `alpha`. Clip rects move
		// with it except the unbounded one.
		void AppendTransformed(const UIDrawList& other, float alpha, const glm::vec2& offset, float scale, const glm::vec2& pivot);

		void Clear();
		bool IsEmpty() const { return m_Commands.empty(); }
		size_t GetCommandCount() const { return m_Commands.size(); }

		// Equal for two lists that build the same geometry; textures and fonts count by GetId(), so a
		// freed texture's reused address never matches.
		uint64_t Hash(uint64_t seed = 0) const;
		// Culls quads that their clip rect or alpha hides and splits a batch where a draw would need
		// more than k_UITextureSlots textures. Reads no texture: it compares their addresses only.
		void Build(UIGeometry& geometry) const;

	private:
		enum class CommandKind : uint32_t
		{
			Rect,
			Image,
			Text,
			NineSlice
		};

		struct Command
		{
			CommandKind Kind = CommandKind::Rect;
			UIRect Rect;                 // Text: Min is the position
			UIRect Clip = k_UINoClip;
			glm::vec4 Color{ 1.0f };
			glm::vec4 BorderColor{ 0.0f };
			glm::vec4 Radii{ 0.0f };
			glm::vec2 UVMin{ 0.0f };
			glm::vec2 UVMax{ 1.0f };
			float BorderWidth = 0.0f;
			float TextSize = 0.0f;
			float MaxWidth = 0.0f;
			float Align = 0.0f;
			float AlignWidth = 0.0f;
			glm::vec4 Borders{ 0.0f };   // NineSlice: texture pixels
			glm::vec2 TextureSize{ 0.0f };
			float Scale = 1.0f;
			bool FillCenter = true;
			bool PointFilter = false;
			Dingo::Texture* Image = nullptr;
			const Dingo::Font* TextFont = nullptr;
			uint32_t TextOffset = 0;
			uint32_t TextLength = 0;
		};

		std::vector<Command> m_Commands;
		std::string m_Text;
	};

}
