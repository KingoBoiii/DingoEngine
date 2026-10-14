#include <doctest/doctest.h>

#include "DingoEngine/UI/DingoUI/DingoUIDrawList.h"

#include <array>
#include <cstddef>

using namespace Dingo;
using namespace Dingo::Internal;

namespace
{
	// Build compares texture addresses only, so stand-ins that are never dereferenced do.
	std::array<std::max_align_t, 40> s_FakeTextureStorage;

	Texture* FakeTexture(size_t index)
	{
		return reinterpret_cast<Texture*>(&s_FakeTextureStorage[index]);
	}

	const UIRect k_Rect = { { 10.0f, 20.0f }, { 50.0f, 40.0f } };
}

TEST_CASE("UIDrawList builds a rect as one quad in one batch")
{
	UIDrawList list;
	list.AddRect(k_Rect, { 1.0f, 0.0f, 0.0f, 1.0f }, 4.0f, 2.0f, { 0.0f, 1.0f, 0.0f, 1.0f });

	UIGeometry geometry;
	list.Build(geometry);
	REQUIRE(geometry.QuadCount == 1);
	REQUIRE(geometry.Vertices.size() == 4);
	REQUIRE(geometry.Batches.size() == 1);
	CHECK(geometry.Batches[0].FirstIndex == 0);
	CHECK(geometry.Batches[0].IndexCount == 6);
	CHECK(geometry.Batches[0].TextureCount == 1);

	CHECK(geometry.Vertices[0].Position == glm::vec2(10.0f, 20.0f));
	CHECK(geometry.Vertices[2].Position == glm::vec2(50.0f, 40.0f));
	CHECK(geometry.Vertices[0].Local == glm::vec4(-20.0f, -10.0f, 20.0f, 10.0f));
	CHECK(geometry.Vertices[0].Radii == glm::vec4(4.0f));
	CHECK(geometry.Vertices[0].Params == glm::vec4(2.0f, 0.0f, static_cast<float>(UIQuadKind::Shape), 0.0f));
}

TEST_CASE("UIDrawList clamps radii and border to half the shorter side")
{
	UIDrawList list;
	list.AddRect(k_Rect, glm::vec4(1.0f), 100.0f, 100.0f, glm::vec4(1.0f));

	UIGeometry geometry;
	list.Build(geometry);
	REQUIRE(geometry.QuadCount == 1);
	CHECK(geometry.Vertices[0].Radii == glm::vec4(10.0f));
	CHECK(geometry.Vertices[0].Params.x == 10.0f);
}

TEST_CASE("UIDrawList culls what its clip or alpha hides")
{
	UIDrawList list;
	list.AddRect(k_Rect, glm::vec4(1.0f), 0.0f, 0.0f, glm::vec4(0.0f), { { 100.0f, 100.0f }, { 200.0f, 200.0f } });
	list.AddRect(k_Rect, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
	list.AddRect({ { 10.0f, 10.0f }, { 10.0f, 30.0f } }, glm::vec4(1.0f));
	list.AddImage(k_Rect, nullptr, glm::vec2(0.0f), glm::vec2(1.0f), glm::vec4(0.0f));
	list.AddRect(k_Rect, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, 1.0f, glm::vec4(1.0f));

	UIGeometry geometry;
	list.Build(geometry);
	CHECK(geometry.CulledQuads == 4);
	CHECK(geometry.QuadCount == 1);
	CHECK(geometry.Batches.size() == 1);
}

TEST_CASE("UIDrawList of only culled quads has no batch")
{
	UIDrawList list;
	list.AddRect(k_Rect, glm::vec4(0.0f));

	UIGeometry geometry;
	list.Build(geometry);
	CHECK(geometry.QuadCount == 0);
	CHECK(geometry.Vertices.empty());
	CHECK(geometry.Batches.empty());
}

TEST_CASE("UIDrawList shares a texture's slot and splits past k_UITextureSlots")
{
	UIDrawList list;
	list.AddImage(k_Rect, FakeTexture(0));
	list.AddRect(k_Rect, glm::vec4(1.0f));
	list.AddImage(k_Rect, FakeTexture(0));

	UIGeometry geometry;
	list.Build(geometry);
	REQUIRE(geometry.Batches.size() == 1);
	CHECK(geometry.Batches[0].TextureCount == 2);
	CHECK(geometry.Batches[0].Textures[0] == nullptr);
	CHECK(geometry.Batches[0].Textures[1] == FakeTexture(0));
	CHECK(geometry.Vertices[0].Params.y == 1.0f);
	CHECK(geometry.Vertices[4].Params.y == 0.0f);
	CHECK(geometry.Vertices[8].Params.y == 1.0f);

	list.Clear();
	CHECK(list.IsEmpty());
	const uint32_t imageCount = k_UITextureSlots + 1;
	for (uint32_t i = 0; i < imageCount; ++i)
		list.AddImage(k_Rect, FakeTexture(i));

	list.Build(geometry);
	REQUIRE(geometry.Batches.size() == 2);
	CHECK(geometry.Batches[0].TextureCount == k_UITextureSlots);
	CHECK(geometry.Batches[0].IndexCount == 6 * (k_UITextureSlots - 1));
	CHECK(geometry.Batches[1].FirstIndex == geometry.Batches[0].IndexCount);
	CHECK(geometry.Batches[1].TextureCount == 3);
	CHECK(geometry.Batches[1].Textures[0] == nullptr);
	CHECK(geometry.Batches[0].IndexCount + geometry.Batches[1].IndexCount == 6 * imageCount);
}

TEST_CASE("UIDrawList hash follows the commands")
{
	auto build = [](const glm::vec4& fill, float radius)
	{
		UIDrawList list;
		list.AddRect(k_Rect, fill, radius);
		list.AddRect({ { 0.0f, 0.0f }, { 8.0f, 8.0f } }, glm::vec4(0.5f));
		return list.Hash();
	};

	const uint64_t base = build(glm::vec4(1.0f), 2.0f);
	CHECK(base == build(glm::vec4(1.0f), 2.0f));
	CHECK(base != build(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f));
	CHECK(base != build(glm::vec4(1.0f), 3.0f));

	UIDrawList list;
	list.AddRect(k_Rect, glm::vec4(1.0f));
	CHECK(list.Hash(1) != list.Hash(2));
	CHECK(list.Hash() != UIDrawList().Hash());
}

TEST_CASE("UIDrawList builds a 9-slice as nine quads with constant corners")
{
	const glm::vec2 textureSize(8.0f, 8.0f);
	const glm::vec4 borders(2.0f, 1.0f, 2.0f, 3.0f);

	auto build = [&](const UIRect& rect, float scale, bool fillCenter, bool point)
	{
		UIDrawList list;
		list.AddNineSlice(rect, FakeTexture(0), textureSize, borders, scale, fillCenter, point);
		UIGeometry geometry;
		list.Build(geometry);
		return geometry;
	};

	for (const glm::vec2 size : { glm::vec2(40.0f, 30.0f), glm::vec2(100.0f, 64.0f) })
	{
		const UIRect rect = { { 10.0f, 20.0f }, { 10.0f + size.x, 20.0f + size.y } };
		const UIGeometry geometry = build(rect, 1.0f, true, false);
		REQUIRE(geometry.QuadCount == 9);
		REQUIRE(geometry.Vertices.size() == 36);
		REQUIRE(geometry.Batches.size() == 1);
		CHECK(geometry.Batches[0].IndexCount == 54);
		CHECK(geometry.Batches[0].TextureCount == 2);

		const auto quadSize = [&](size_t quad) { return geometry.Vertices[quad * 4 + 2].Position - geometry.Vertices[quad * 4].Position; };
		CHECK(quadSize(0) == glm::vec2(2.0f, 1.0f));
		CHECK(quadSize(2) == glm::vec2(2.0f, 1.0f));
		CHECK(quadSize(6) == glm::vec2(2.0f, 3.0f));
		CHECK(quadSize(8) == glm::vec2(2.0f, 3.0f));
		CHECK(quadSize(4) == glm::vec2(size.x - 4.0f, size.y - 4.0f));
		CHECK(geometry.Vertices[0].Position == rect.Min);
		CHECK(geometry.Vertices[8 * 4 + 2].Position == rect.Max);
	}

	const UIGeometry geometry = build({ { 0.0f, 0.0f }, { 40.0f, 30.0f } }, 1.0f, true, false);
	// Image UVs: (0, 0) is the bottom-left, so the top-left cell takes the picture's top-left corner.
	CHECK(geometry.Vertices[0].TexCoord == glm::vec2(0.0f, 1.0f));
	CHECK(geometry.Vertices[2].TexCoord == glm::vec2(0.25f, 1.0f - 1.0f / 8.0f));
	CHECK(geometry.Vertices[8 * 4 + 2].TexCoord == glm::vec2(1.0f, 0.0f));
	CHECK(geometry.Vertices[6 * 4 + 3].TexCoord == glm::vec2(0.0f, 0.0f));
	CHECK(geometry.Vertices[4 * 4].TexCoord == glm::vec2(0.25f, 1.0f - 1.0f / 8.0f));
	CHECK(geometry.Vertices[4 * 4 + 2].TexCoord == glm::vec2(0.75f, 3.0f / 8.0f));
	CHECK(geometry.Vertices[0].Params.z == static_cast<float>(UIQuadKind::Shape));

	const UIGeometry scaled = build({ { 0.0f, 0.0f }, { 40.0f, 30.0f } }, 2.5f, true, true);
	CHECK(scaled.Vertices[2].Position == glm::vec2(5.0f, 3.0f));
	CHECK(scaled.Vertices[0].Params.z == static_cast<float>(UIQuadKind::ShapePoint));

	const UIGeometry hole = build({ { 0.0f, 0.0f }, { 40.0f, 30.0f } }, 1.0f, false, false);
	CHECK(hole.QuadCount == 8);

	const UIGeometry tiny = build({ { 0.0f, 0.0f }, { 3.0f, 30.0f } }, 1.0f, true, false);
	REQUIRE(tiny.QuadCount > 0);
	float widest = 0.0f;
	for (const UIVertex& vertex : tiny.Vertices)
		widest = (std::max)(widest, vertex.Position.x);
	CHECK(widest == 3.0f);

	UIDrawList clipped;
	clipped.AddNineSlice({ { 0.0f, 0.0f }, { 40.0f, 30.0f } }, FakeTexture(0), textureSize, borders, 1.0f, true, false, glm::vec4(1.0f), { { 100.0f, 100.0f }, { 200.0f, 200.0f } });
	clipped.AddNineSlice({ { 0.0f, 0.0f }, { 40.0f, 30.0f } }, nullptr, textureSize, borders);
	UIGeometry culled;
	clipped.Build(culled);
	CHECK(culled.QuadCount == 0);
	CHECK(culled.CulledQuads == 2);
}
