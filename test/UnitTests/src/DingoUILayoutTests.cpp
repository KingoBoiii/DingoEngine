#include <doctest/doctest.h>

#include "DingoEngine/UI/DingoUI/DingoUILayout.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace Dingo;
using namespace Dingo::Internal;

namespace
{
	constexpr float k_Advance = 0.5f;      // of the size, per character
	constexpr float k_LineHeight = 1.25f;  // of the size, between baselines
	const glm::vec2 k_Viewport = { 1000.0f, 600.0f };
	const glm::vec4 k_Fill = { 0.2f, 0.4f, 0.6f, 1.0f };

	// Fixed-advance text: greedy word wrap at spaces, a word wider than the line stays whole.
	glm::vec2 MeasureFixed(const Font*, std::string_view text, float size, float maxWidth)
	{
		if (text.empty())
			return glm::vec2(0.0f);

		const float advance = size * k_Advance;
		float widest = 0.0f;
		uint32_t lines = 1;
		float line = 0.0f;
		size_t begin = 0;
		while (begin < text.size())
		{
			size_t end = text.find(' ', begin);
			const bool last = end == std::string_view::npos;
			if (last)
				end = text.size();
			const float word = static_cast<float>(end - begin) * advance;
			const float withSpace = line > 0.0f ? line + advance + word : word;
			if (maxWidth > 0.0f && line > 0.0f && withSpace > maxWidth)
			{
				widest = (std::max)(widest, line);
				++lines;
				line = word;
			}
			else
			{
				line = withSpace;
			}
			begin = end + 1;
		}
		widest = (std::max)(widest, line);
		return { widest, size + static_cast<float>(lines - 1) * size * k_LineHeight };
	}

	const UITextMeasureFn k_Measure = [](const Font* font, std::string_view text, float size, float maxWidth) { return MeasureFixed(font, text, size, maxWidth); };

	const Font* FakeFont()
	{
		static int s_Storage = 0;
		return reinterpret_cast<const Font*>(&s_Storage);
	}

	struct Frame
	{
		UILayoutTree Tree;

		explicit Frame(const glm::vec2& viewport = k_Viewport) { Tree.Begin(viewport); }

		void Layout() { Tree.Layout(k_Measure); }

		UIRect Rect(std::string_view name) const
		{
			const UIElement* element = Tree.FindByName(name);
			REQUIRE(element != nullptr);
			return element->Rect;
		}

		bool Text(std::string_view text, const UITextParams& params = UITextParams()) { return Tree.AddText(text, params, FakeFont()); }
	};

	UIBoxParams Sized(const char* id, const UISize& width, const UISize& height)
	{
		return UIBoxParams().SetId(id).SetSize(width, height);
	}

	void Leaf(Frame& frame, const char* id, const UISize& width, const UISize& height)
	{
		REQUIRE(frame.Tree.BeginBox(Sized(id, width, height)));
		REQUIRE(frame.Tree.EndBox());
	}

	void CheckRect(const UIRect& rect, float minX, float minY, float maxX, float maxY)
	{
		CHECK(rect.Min.x == doctest::Approx(minX));
		CHECK(rect.Min.y == doctest::Approx(minY));
		CHECK(rect.Max.x == doctest::Approx(maxX));
		CHECK(rect.Max.y == doctest::Approx(maxY));
	}
}

TEST_CASE("UILayout places a fixed root at each of the nine anchors")
{
	struct Case { UIAnchor Anchor; float X; float Y; };
	const Case cases[] = {
		{ UIAnchor::TopLeft, 0.0f, 0.0f },       { UIAnchor::Top, 400.0f, 0.0f },       { UIAnchor::TopRight, 800.0f, 0.0f },
		{ UIAnchor::Left, 0.0f, 250.0f },        { UIAnchor::Center, 400.0f, 250.0f },  { UIAnchor::Right, 800.0f, 250.0f },
		{ UIAnchor::BottomLeft, 0.0f, 500.0f },  { UIAnchor::Bottom, 400.0f, 500.0f },  { UIAnchor::BottomRight, 800.0f, 500.0f },
	};

	for (const Case& c : cases)
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetAnchor(c.Anchor).SetLayout(UIBoxParams().SetSize(UISize::Fixed(200), UISize::Fixed(100)))));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CAPTURE(static_cast<int>(c.Anchor));
		CheckRect(frame.Rect("root"), c.X, c.Y, c.X + 200.0f, c.Y + 100.0f);
	}
}

TEST_CASE("UILayout stretches a root over the viewport and offsets it")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetAnchor(UIAnchor::Stretch).SetOffset({ 10.0f, 20.0f }).SetLayout(UIBoxParams().SetSize(UISize::Fixed(5), UISize::Fixed(5)))));
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();
	CheckRect(frame.Rect("root"), 10.0f, 20.0f, 1010.0f, 620.0f);
}

TEST_CASE("UILayout offsets from the anchor and honours a pivot")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("a", UIRootParams().SetAnchor(UIAnchor::BottomRight).SetOffset({ -16.0f, -16.0f }).SetLayout(UIBoxParams().SetSize(UISize::Fixed(200), UISize::Fixed(100)))));
	REQUIRE(frame.Tree.EndRoot());
	REQUIRE(frame.Tree.BeginRoot("b", UIRootParams().SetAnchor(UIAnchor::Center).SetPivot({ 0.0f, 0.0f }).SetLayout(UIBoxParams().SetSize(UISize::Fixed(200), UISize::Fixed(100)))));
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();
	CheckRect(frame.Rect("a"), 784.0f, 484.0f, 984.0f, 584.0f);
	CheckRect(frame.Rect("b"), 500.0f, 300.0f, 700.0f, 400.0f);
}

TEST_CASE("UILayout lays a column out with gap and padding")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetWidth(UISize::Fixed(300)).SetDirection(UIDirection::Column).SetGap(10).SetPadding(20))));
	Leaf(frame, "a", UISize::Fixed(100), UISize::Fixed(40));
	Leaf(frame, "b", UISize::Fixed(150), UISize::Fixed(50));
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();

	CheckRect(frame.Rect("root"), 0.0f, 0.0f, 300.0f, 140.0f);
	CheckRect(frame.Rect("a"), 20.0f, 20.0f, 120.0f, 60.0f);
	CheckRect(frame.Rect("b"), 20.0f, 70.0f, 170.0f, 120.0f);
}

TEST_CASE("UILayout lays a row out with gap and padding")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetDirection(UIDirection::Row).SetGap(8).SetPadding(4, 6, 4, 6))));
	Leaf(frame, "a", UISize::Fixed(30), UISize::Fixed(20));
	Leaf(frame, "b", UISize::Fixed(50), UISize::Fixed(40));
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();

	CheckRect(frame.Rect("root"), 0.0f, 0.0f, 96.0f, 52.0f);
	CheckRect(frame.Rect("a"), 4.0f, 6.0f, 34.0f, 26.0f);
	CheckRect(frame.Rect("b"), 42.0f, 6.0f, 92.0f, 46.0f);
}

TEST_CASE("UILayout grow shares the leftover by weight")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetSize(UISize::Fixed(410), UISize::Fixed(50)).SetDirection(UIDirection::Row).SetGap(10))));
	Leaf(frame, "fixed", UISize::Fixed(50), UISize::Grow());
	Leaf(frame, "one", UISize::Grow(1), UISize::Fit());
	Leaf(frame, "three", UISize::Grow(3), UISize::Fit());
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();

	CheckRect(frame.Rect("fixed"), 0.0f, 0.0f, 50.0f, 50.0f);
	CheckRect(frame.Rect("one"), 60.0f, 0.0f, 145.0f, 0.0f);
	CheckRect(frame.Rect("three"), 155.0f, 0.0f, 410.0f, 0.0f);
}

TEST_CASE("UILayout grow honours min and max")
{
	SUBCASE("a max freezes one and the other takes the rest")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetSize(UISize::Fixed(400), UISize::Fixed(10)).SetDirection(UIDirection::Row))));
		Leaf(frame, "a", UISize::Grow().SetMax(100), UISize::Fit());
		Leaf(frame, "b", UISize::Grow(), UISize::Fit());
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CHECK(frame.Rect("a").Max.x - frame.Rect("a").Min.x == doctest::Approx(100.0f));
		CHECK(frame.Rect("b").Max.x - frame.Rect("b").Min.x == doctest::Approx(300.0f));
	}
	SUBCASE("a min freezes one and the other takes the rest")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetSize(UISize::Fixed(400), UISize::Fixed(10)).SetDirection(UIDirection::Row))));
		Leaf(frame, "a", UISize::Grow().SetMin(250), UISize::Fit());
		Leaf(frame, "b", UISize::Grow(), UISize::Fit());
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CHECK(frame.Rect("a").Max.x - frame.Rect("a").Min.x == doctest::Approx(250.0f));
		CHECK(frame.Rect("b").Max.x - frame.Rect("b").Min.x == doctest::Approx(150.0f));
	}
	SUBCASE("grow across the direction fills the container")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetSize(UISize::Fixed(200), UISize::Fixed(120)).SetDirection(UIDirection::Row))));
		Leaf(frame, "a", UISize::Fixed(20), UISize::Grow());
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CheckRect(frame.Rect("a"), 0.0f, 0.0f, 20.0f, 120.0f);
	}
}

TEST_CASE("UILayout justify and align place children in the room left over")
{
	const auto build = [](UIJustify justify, UIAlign align)
	{
		auto frame = std::make_unique<Frame>();
		REQUIRE(frame->Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetSize(UISize::Fixed(300), UISize::Fixed(100)).SetDirection(UIDirection::Row).SetJustify(justify).SetAlign(align))));
		Leaf(*frame, "a", UISize::Fixed(40), UISize::Fixed(20));
		Leaf(*frame, "b", UISize::Fixed(60), UISize::Fixed(20));
		REQUIRE(frame->Tree.EndRoot());
		frame->Layout();
		return frame;
	};

	CheckRect(build(UIJustify::Center, UIAlign::Start)->Rect("a"), 100.0f, 0.0f, 140.0f, 20.0f);
	CheckRect(build(UIJustify::End, UIAlign::Start)->Rect("b"), 240.0f, 0.0f, 300.0f, 20.0f);
	CheckRect(build(UIJustify::SpaceBetween, UIAlign::Start)->Rect("a"), 0.0f, 0.0f, 40.0f, 20.0f);
	CheckRect(build(UIJustify::SpaceBetween, UIAlign::Start)->Rect("b"), 240.0f, 0.0f, 300.0f, 20.0f);
	CheckRect(build(UIJustify::Start, UIAlign::Center)->Rect("a"), 0.0f, 40.0f, 40.0f, 60.0f);
	CheckRect(build(UIJustify::Start, UIAlign::End)->Rect("a"), 0.0f, 80.0f, 40.0f, 100.0f);
	CHECK(build(UIJustify::Start, UIAlign::Stretch)->Rect("a").Max.y == 20.0f);
}

TEST_CASE("UILayout align stretch grows fit children across the direction")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetSize(UISize::Fixed(300), UISize::Fixed(100)).SetDirection(UIDirection::Column).SetAlign(UIAlign::Stretch))));
	Leaf(frame, "fit", UISize::Fit(), UISize::Fixed(20));
	Leaf(frame, "fixed", UISize::Fixed(50), UISize::Fixed(20));
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();
	CheckRect(frame.Rect("fit"), 0.0f, 0.0f, 300.0f, 20.0f);
	CheckRect(frame.Rect("fixed"), 0.0f, 20.0f, 50.0f, 40.0f);
}

TEST_CASE("UILayout fits a box around its text")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetPadding(8))));
	REQUIRE(frame.Text("Hello", UITextParams().SetSize(20.0f)));
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();

	CheckRect(frame.Rect("root"), 0.0f, 0.0f, 66.0f, 36.0f);
	const UIElement& text = frame.Tree.GetElements()[1];
	CHECK(text.Kind == UIElementKind::Text);
	CheckRect(text.Rect, 8.0f, 8.0f, 58.0f, 28.0f);
}

TEST_CASE("UILayout wraps text at the width its parent grants and grows taller")
{
	const UITextParams text = UITextParams().SetSize(20.0f);
	const char* sentence = "aaaa bbbb cccc";

	Frame unwrapped;
	REQUIRE(unwrapped.Tree.BeginRoot("root", UIRootParams()));
	REQUIRE(unwrapped.Text(sentence, text));
	REQUIRE(unwrapped.Tree.EndRoot());
	unwrapped.Layout();
	CheckRect(unwrapped.Rect("root"), 0.0f, 0.0f, 140.0f, 20.0f);

	SUBCASE("a fixed-width column wraps its text")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetWidth(UISize::Fixed(100)))));
		REQUIRE(frame.Text(sentence, text));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CheckRect(frame.Rect("root"), 0.0f, 0.0f, 100.0f, 45.0f);
		CHECK(frame.Rect("root").Max.y > unwrapped.Rect("root").Max.y);
	}
	SUBCASE("a max size on a fit box wraps its text")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetWidth(UISize::Fit().SetMax(100)))));
		REQUIRE(frame.Text(sentence, text));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CheckRect(frame.Rect("root"), 0.0f, 0.0f, 100.0f, 45.0f);
	}
	SUBCASE("Wrap off keeps the text on one line")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetWidth(UISize::Fixed(100)))));
		REQUIRE(frame.Text(sentence, UITextParams(text).SetWrap(false)));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CheckRect(frame.Rect("root"), 0.0f, 0.0f, 100.0f, 20.0f);
	}
	SUBCASE("MaxWidth wraps a text in a roomy box")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams()));
		REQUIRE(frame.Text(sentence, UITextParams(text).SetMaxWidth(100.0f)));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CheckRect(frame.Rect("root"), 0.0f, 0.0f, 90.0f, 45.0f);
	}
	SUBCASE("a row shrinks its text toward the widest word before overflowing")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetWidth(UISize::Fixed(100)).SetDirection(UIDirection::Row))));
		REQUIRE(frame.Text(sentence, text));
		Leaf(frame, "icon", UISize::Fixed(40), UISize::Fixed(10));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		const UIElement& wrapped = frame.Tree.GetElements()[1];
		CHECK(wrapped.Rect.Max.x - wrapped.Rect.Min.x == doctest::Approx(60.0f));
		CHECK(frame.Rect("root").Max.y == doctest::Approx(70.0f));
		CHECK(frame.Rect("icon").Min.x == doctest::Approx(60.0f));
	}
}

TEST_CASE("UILayout percent sizes take a fraction of the content area")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetSize(UISize::Fixed(400), UISize::Fixed(200)).SetPadding(20))));
	Leaf(frame, "half", UISize::Percent(0.5f), UISize::Percent(0.25f));
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();
	CheckRect(frame.Rect("half"), 20.0f, 20.0f, 200.0f, 60.0f);
}

TEST_CASE("UILayout percent sizes of a root follow the viewport")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetSize(UISize::Percent(0.5f), UISize::Percent(1.0f)))));
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();
	CheckRect(frame.Rect("root"), 0.0f, 0.0f, 500.0f, 600.0f);
}

TEST_CASE("UILayout clamps sizes to min and max")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetWidth(UISize::Fit().SetMin(80)).SetHeight(UISize::Fit().SetMax(30)).SetDirection(UIDirection::Row))));
	Leaf(frame, "a", UISize::Fixed(30), UISize::Fixed(50));
	Leaf(frame, "b", UISize::Fixed(500).SetMax(40), UISize::Fixed(5).SetMin(10));
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();

	CheckRect(frame.Rect("root"), 0.0f, 0.0f, 80.0f, 30.0f);
	CheckRect(frame.Rect("a"), 0.0f, 0.0f, 30.0f, 50.0f);
	CheckRect(frame.Rect("b"), 30.0f, 0.0f, 70.0f, 10.0f);
}

TEST_CASE("UILayout scales a root with the screen")
{
	CHECK(ComputeUIScale(UIScale::ScaleWithScreen(), { 960.0f, 540.0f }) == doctest::Approx(0.5f));
	CHECK(ComputeUIScale(UIScale::ScaleWithScreen(), { 3840.0f, 2160.0f }) == doctest::Approx(2.0f));
	CHECK(ComputeUIScale(UIScale::ScaleWithScreen({ 1920.0f, 1080.0f }, 0.0f), { 960.0f, 1080.0f }) == doctest::Approx(0.5f));
	CHECK(ComputeUIScale(UIScale::ScaleWithScreen({ 1920.0f, 1080.0f }, 1.0f), { 960.0f, 1080.0f }) == doctest::Approx(1.0f));
	CHECK(ComputeUIScale(UIScale::ScaleWithScreen({ 1920.0f, 1080.0f }, 0.5f), { 960.0f, 1080.0f }) == doctest::Approx(std::sqrt(0.5f)));
	CHECK(ComputeUIScale(UIScale::ConstantPixel(), { 960.0f, 540.0f }) == 1.0f);
	CHECK(ComputeUIScale(UIScale::ConstantPixel(2.0f), { 960.0f, 540.0f }) == 2.0f);

	SUBCASE("a 1920x1080 reference on a 960x540 target halves every length")
	{
		Frame frame({ 960.0f, 540.0f });
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetAnchor(UIAnchor::TopRight).SetOffset({ -20.0f, 20.0f }).SetScale(UIScale::ScaleWithScreen()).SetLayout(UIBoxParams().SetSize(UISize::Fixed(200), UISize::Fit()).SetPadding(10).SetGap(20))));
		Leaf(frame, "a", UISize::Percent(1.0f), UISize::Fixed(40));
		REQUIRE(frame.Text("Hello", UITextParams().SetSize(40.0f)));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();

		CheckRect(frame.Rect("root"), 960.0f - 10.0f - 100.0f, 10.0f, 960.0f - 10.0f, 10.0f + 5.0f + 20.0f + 10.0f + 20.0f + 5.0f);
		CheckRect(frame.Rect("a"), 850.0f + 5.0f, 15.0f, 945.0f, 35.0f);
		CHECK(frame.Tree.GetElements()[2].TextSize == doctest::Approx(20.0f));
		CHECK(frame.Tree.GetElements()[2].Rect.Min.y == doctest::Approx(15.0f + 20.0f + 10.0f));
	}
	SUBCASE("a constant pixel scale multiplies lengths")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetScale(UIScale::ConstantPixel(2.0f)).SetLayout(UIBoxParams().SetSize(UISize::Fixed(100), UISize::Fixed(30)))));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CheckRect(frame.Rect("root"), 0.0f, 0.0f, 200.0f, 60.0f);
	}
}

TEST_CASE("UILayout snaps final rects to whole pixels")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams().SetLayout(UIBoxParams().SetSize(UISize::Fixed(100), UISize::Fixed(10)).SetDirection(UIDirection::Row).SetGap(0.5f))));
	Leaf(frame, "a", UISize::Grow(), UISize::Fit());
	Leaf(frame, "b", UISize::Grow(), UISize::Fit());
	Leaf(frame, "c", UISize::Grow(), UISize::Fit());
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();

	for (const char* name : { "a", "b", "c" })
	{
		const UIRect rect = frame.Rect(name);
		CHECK(rect.Min.x == std::round(rect.Min.x));
		CHECK(rect.Max.x == std::round(rect.Max.x));
	}
	CHECK(frame.Rect("a").Max.x <= frame.Rect("b").Min.x + 1.0f);
	CHECK(frame.Rect("c").Max.x == 100.0f);
}

TEST_CASE("UILayout sizes an image from its texture and keeps its aspect")
{
	SUBCASE("fit is the texture size")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams()));
		REQUIRE(frame.Tree.AddImage(nullptr, { 200.0f, 100.0f }, UIImageParams()));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CheckRect(frame.Tree.GetElements()[1].Rect, 0.0f, 0.0f, 200.0f, 100.0f);
	}
	SUBCASE("a fixed height derives the width")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams()));
		REQUIRE(frame.Tree.AddImage(nullptr, { 200.0f, 100.0f }, UIImageParams().SetSize(UISize::Fit(), UISize::Fixed(50))));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CheckRect(frame.Rect("root"), 0.0f, 0.0f, 100.0f, 50.0f);
	}
	SUBCASE("a fixed width derives the height")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams()));
		REQUIRE(frame.Tree.AddImage(nullptr, { 200.0f, 100.0f }, UIImageParams().SetSize(UISize::Fixed(40), UISize::Fit())));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();
		CheckRect(frame.Rect("root"), 0.0f, 0.0f, 40.0f, 20.0f);
	}
	SUBCASE("a box of another shape letterboxes the drawing")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams()));
		REQUIRE(frame.Tree.AddImage(nullptr, { 200.0f, 100.0f }, UIImageParams().SetSize(UISize::Fixed(100), UISize::Fixed(100))));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();

		UIDrawList list;
		frame.Tree.Emit(list);
		UIGeometry geometry;
		list.Build(geometry);
		REQUIRE(geometry.Vertices.size() == 4);
		CHECK(geometry.Vertices[0].Position == glm::vec2(0.0f, 25.0f));
		CHECK(geometry.Vertices[2].Position == glm::vec2(100.0f, 75.0f));
	}
	SUBCASE("PreserveAspect off fills the box")
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("root", UIRootParams()));
		REQUIRE(frame.Tree.AddImage(nullptr, { 200.0f, 100.0f }, UIImageParams().SetSize(UISize::Fixed(100), UISize::Fixed(100)).SetPreserveAspect(false)));
		REQUIRE(frame.Tree.EndRoot());
		frame.Layout();

		UIDrawList list;
		frame.Tree.Emit(list);
		UIGeometry geometry;
		list.Build(geometry);
		REQUIRE(geometry.Vertices.size() == 4);
		CHECK(geometry.Vertices[0].Position == glm::vec2(0.0f, 0.0f));
		CHECK(geometry.Vertices[2].Position == glm::vec2(100.0f, 100.0f));
	}
}

TEST_CASE("UILayout emits roots by layer, then declaration order, with clipping")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("top", UIRootParams().SetLayer(1).SetLayout(UIBoxParams().SetSize(UISize::Fixed(10), UISize::Fixed(10)).SetBackground(k_Fill))));
	REQUIRE(frame.Tree.EndRoot());
	REQUIRE(frame.Tree.BeginRoot("bottom", UIRootParams().SetAnchor(UIAnchor::BottomRight).SetLayout(UIBoxParams().SetSize(UISize::Fixed(20), UISize::Fixed(20)).SetBackground(k_Fill))));
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();

	UIDrawList list;
	frame.Tree.Emit(list);
	UIGeometry geometry;
	list.Build(geometry);
	REQUIRE(geometry.QuadCount == 2);
	CHECK(geometry.Vertices[0].Position == glm::vec2(980.0f, 580.0f));
	CHECK(geometry.Vertices[4].Position == glm::vec2(0.0f, 0.0f));
}

TEST_CASE("UILayout clips the children of a clipping box")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams()));
	REQUIRE(frame.Tree.BeginBox(UIBoxParams().SetId("clip").SetSize(UISize::Fixed(50), UISize::Fixed(50)).SetClipChildren(true)));
	REQUIRE(frame.Tree.BeginBox(UIBoxParams().SetId("big").SetSize(UISize::Fixed(200), UISize::Fixed(200)).SetBackground(k_Fill)));
	REQUIRE(frame.Tree.EndBox());
	REQUIRE(frame.Tree.EndBox());
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();

	const UIElement* big = frame.Tree.FindByName("big");
	REQUIRE(big != nullptr);
	CheckRect(big->Clip, 0.0f, 0.0f, 50.0f, 50.0f);
	CheckRect(big->Rect, 0.0f, 0.0f, 200.0f, 200.0f);

	std::vector<UIRect> rects;
	frame.Tree.CollectBlockingRects(rects);
	REQUIRE(rects.size() == 1);
	CheckRect(rects[0], 0.0f, 0.0f, 50.0f, 50.0f);
}

TEST_CASE("UILayout blocks input only where something draws, in roots that block")
{
	Frame frame;
	REQUIRE(frame.Tree.BeginRoot("menu", UIRootParams().SetLayout(UIBoxParams().SetSize(UISize::Fixed(100), UISize::Fixed(100)))));
	Leaf(frame, "bare", UISize::Fixed(10), UISize::Fixed(10));
	REQUIRE(frame.Tree.BeginBox(UIBoxParams().SetSize(UISize::Fixed(30), UISize::Fixed(30)).SetBackground(k_Fill)));
	REQUIRE(frame.Tree.EndBox());
	REQUIRE(frame.Tree.EndRoot());
	REQUIRE(frame.Tree.BeginRoot("hud", UIRootParams().SetBlocksInput(false).SetAnchor(UIAnchor::BottomRight).SetLayout(UIBoxParams().SetSize(UISize::Fixed(100), UISize::Fixed(100)).SetBackground(k_Fill))));
	REQUIRE(frame.Tree.EndRoot());
	frame.Layout();

	std::vector<UIRect> rects;
	frame.Tree.CollectBlockingRects(rects);
	REQUIRE(rects.size() == 1);
	CheckRect(rects[0], 0.0f, 10.0f, 30.0f, 40.0f);
}

TEST_CASE("UILayout reports unmatched Begin and End calls")
{
	Frame frame;
	CHECK_FALSE(frame.Tree.BeginBox(UIBoxParams()));
	CHECK_FALSE(frame.Text("outside"));
	CHECK_FALSE(frame.Tree.EndRoot());
	CHECK_FALSE(frame.Tree.EndBox());

	REQUIRE(frame.Tree.BeginRoot("root", UIRootParams()));
	CHECK_FALSE(frame.Tree.BeginRoot("nested", UIRootParams()));
	CHECK_FALSE(frame.Tree.EndBox());
	REQUIRE(frame.Tree.BeginBox(UIBoxParams()));
	CHECK_FALSE(frame.Tree.EndRoot());
	CHECK(frame.Tree.GetOpenCount() == 2);
	CHECK(frame.Tree.CloseAll() == 2);
	CHECK(frame.Tree.GetOpenCount() == 0);

	frame.Layout();
	CHECK(frame.Tree.GetElements().size() == 2);
}

TEST_CASE("UILayout keeps ids stable between frames")
{
	const auto ids = [](bool extra)
	{
		Frame frame;
		REQUIRE(frame.Tree.BeginRoot("menu", UIRootParams()));
		if (extra)
			Leaf(frame, "before", UISize::Fixed(1), UISize::Fixed(1));
		Leaf(frame, "named", UISize::Fixed(1), UISize::Fixed(1));
		REQUIRE(frame.Tree.EndRoot());
		return frame.Tree.FindByName("named")->Id;
	};

	CHECK(ids(false) == ids(false));
	CHECK(ids(false) != 0);
	CHECK(ids(true) == ids(false));
}

TEST_CASE("UILayout reuses its storage across frames")
{
	UILayoutTree tree;
	for (int frameIndex = 0; frameIndex < 3; ++frameIndex)
	{
		tree.Begin(k_Viewport);
		REQUIRE(tree.BeginRoot("root", UIRootParams()));
		for (int i = 0; i < 10; ++i)
			REQUIRE(tree.AddText("item", UITextParams(), FakeFont()));
		REQUIRE(tree.EndRoot());
		tree.Layout(k_Measure);
		CHECK(tree.GetElements().size() == 11);
	}
}
