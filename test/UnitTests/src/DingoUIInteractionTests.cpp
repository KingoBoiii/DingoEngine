#include <doctest/doctest.h>

#include "DingoEngine/UI/DingoUI/DingoUILayout.h"
#include "DingoEngine/UI/DingoUI/DingoUIState.h"

#include <string>
#include <unordered_set>

using namespace Dingo;
using namespace Dingo::Internal;

namespace
{
	const glm::vec2 k_Viewport = { 1000.0f, 600.0f };
	const glm::vec4 k_Fill = { 0.3f, 0.4f, 0.6f, 1.0f };

	const UITextMeasureFn k_Measure = [](const Font*, std::string_view, float, float) { return glm::vec2(0.0f); };

	struct UITestScene
	{
		UILayoutTree Tree;

		UITestScene() { Tree.Begin(k_Viewport); }

		void OpenRoot(const char* name, const glm::vec2& offset, const glm::vec2& size, int layer = 0, bool blocks = true, bool modal = false)
		{
			REQUIRE(Tree.BeginRoot(name, UIRootParams().SetOffset(offset).SetLayer(layer).SetBlocksInput(blocks).SetModal(modal)
				.SetLayout(UIBoxParams().SetSize(UISize::Fixed(size.x), UISize::Fixed(size.y)))));
		}

		void CloseRoot() { REQUIRE(Tree.EndRoot()); }

		uint64_t Widget(const char* label, const glm::vec2& size, bool interactive = true, bool draws = true)
		{
			UIWidgetInfo info;
			info.Id = Tree.MakeId(label);
			info.NameHash = UILayoutTree::HashName(label);
			info.Interactive = interactive;
			UIBoxParams box = UIBoxParams().SetSize(UISize::Fixed(size.x), UISize::Fixed(size.y));
			if (draws)
				box.Background = k_Fill;
			REQUIRE(Tree.BeginBox(box, &info));
			REQUIRE(Tree.EndBox());
			return info.Id;
		}

		UIHit Hit(const glm::vec2& point)
		{
			Tree.CloseAll();
			Tree.Layout(k_Measure);
			return Tree.HitTest(point);
		}
	};

	uint64_t IdOf(const char* label)
	{
		UITestScene scene;
		scene.OpenRoot("r", { 0.0f, 0.0f }, { 100.0f, 100.0f });
		return scene.Tree.MakeId(label);
	}
}

TEST_CASE("UI ids hash the label and stay the same between frames")
{
	CHECK(IdOf("Play") == IdOf("Play"));
	CHECK(IdOf("Play") != IdOf("Quit"));
	CHECK(IdOf("Play") != 0);
	CHECK(IdOf("OK##first") != IdOf("OK##second"));
	CHECK(IdOf("OK##first") == IdOf("OK##first"));
	CHECK(UILayoutTree::HashName("Play") == UILayoutTree::HashName("Play"));
}

TEST_CASE("UI ids are 0 with nothing open and differ between containers")
{
	UILayoutTree empty;
	empty.Begin(k_Viewport);
	CHECK(empty.MakeId("Play") == 0);

	UITestScene scene;
	scene.OpenRoot("a", { 0.0f, 0.0f }, { 100.0f, 100.0f });
	const uint64_t inA = scene.Tree.MakeId("Play");
	scene.CloseRoot();
	scene.OpenRoot("b", { 0.0f, 0.0f }, { 100.0f, 100.0f });
	CHECK(scene.Tree.MakeId("Play") != inA);
}

TEST_CASE("UI PushId scopes ids until PopId")
{
	UITestScene scene;
	scene.OpenRoot("r", { 0.0f, 0.0f }, { 100.0f, 100.0f });
	const uint64_t plain = scene.Tree.MakeId("Row");

	std::unordered_set<uint64_t> seen;
	for (int i = 0; i < 20; ++i)
	{
		scene.Tree.PushId(i);
		const uint64_t id = scene.Tree.MakeId("Row");
		CHECK(id != plain);
		CHECK(seen.insert(id).second);
		CHECK(scene.Tree.PopId());
		CHECK(scene.Tree.MakeId("Row") == plain);
	}

	scene.Tree.PushId("left");
	const uint64_t left = scene.Tree.MakeId("Row");
	CHECK(scene.Tree.PopId());
	scene.Tree.PushId("right");
	CHECK(scene.Tree.MakeId("Row") != left);
	CHECK(scene.Tree.PopId());

	scene.Tree.PushId("a");
	scene.Tree.PushId("b");
	const uint64_t nested = scene.Tree.MakeId("Row");
	CHECK(scene.Tree.PopId());
	CHECK(scene.Tree.MakeId("Row") != nested);
	CHECK(scene.Tree.PopId());
	CHECK(scene.Tree.MakeId("Row") == plain);
}

TEST_CASE("UI PopId cannot pop an id pushed outside the container, and a container drops its pushes")
{
	UITestScene scene;
	scene.OpenRoot("r", { 0.0f, 0.0f }, { 100.0f, 100.0f });
	scene.Tree.PushId("outer");
	const uint64_t outer = scene.Tree.MakeId("Row");

	REQUIRE(scene.Tree.BeginBox(UIBoxParams()));
	CHECK(!scene.Tree.PopId());
	scene.Tree.PushId("inner");
	scene.Tree.PushId("inner2");
	CHECK(scene.Tree.GetIdStackDepth() == 3);
	REQUIRE(scene.Tree.EndBox());
	CHECK(scene.Tree.GetIdStackDepth() == 1);
	CHECK(scene.Tree.MakeId("Row") == outer);
	CHECK(scene.Tree.PopId());
	CHECK(!scene.Tree.PopId());
}

TEST_CASE("UIStateMap keeps what is touched and flags a second touch in one frame")
{
	UIStateMap map;
	const auto first = map.Touch(7, 0);
	CHECK(first.IsNew);
	CHECK(!first.DeclaredAlready);
	first.State->ScrollOffset.y = 12.0f;

	const auto again = map.Touch(7, 0);
	CHECK(!again.IsNew);
	CHECK(again.DeclaredAlready);

	const auto next = map.Touch(7, 1);
	CHECK(!next.IsNew);
	CHECK(!next.DeclaredAlready);
	CHECK(next.State->ScrollOffset.y == doctest::Approx(12.0f));
	CHECK(map.Find(7) == next.State);
	CHECK(map.Find(8) == nullptr);
	CHECK(next.State->LastSeenFrame == 1);
}

TEST_CASE("UIStateMap evicts what no frame declared for the max age")
{
	UIStateMap map;
	map.Touch(1, 0);
	map.Touch(2, 0);
	map.Touch(2, 1);
	CHECK(map.Evict(0) == 0);
	CHECK(map.Evict(1) == 0);
	CHECK(map.Size() == 2);

	CHECK(map.Evict(2) == 1);
	CHECK(map.Find(1) == nullptr);
	CHECK(map.Find(2) != nullptr);

	CHECK(map.Evict(3) == 1);
	CHECK(map.Size() == 0);
}

TEST_CASE("UIStateMap stays small while widgets come and go")
{
	UIStateMap map;
	for (uint64_t frame = 0; frame < 2000; ++frame)
	{
		map.Touch(frame + 1, frame);
		map.Touch(frame + 2, frame);
		map.Touch(100000, frame);
		map.Evict(frame);
		CHECK(map.Size() <= 5);
	}
	CHECK(map.Find(100000) != nullptr);
}

TEST_CASE("UI hit-test finds the interactive element under the point")
{
	UITestScene scene;
	scene.OpenRoot("r", { 100.0f, 100.0f }, { 300.0f, 200.0f });
	const uint64_t a = scene.Widget("a", { 100.0f, 50.0f });
	const uint64_t b = scene.Widget("b", { 100.0f, 50.0f });
	scene.CloseRoot();
	scene.Tree.Layout(k_Measure);

	CHECK(scene.Tree.HitTest({ 150.0f, 120.0f }).HoverId == a);
	CHECK(scene.Tree.HitTest({ 150.0f, 170.0f }).HoverId == b);
	const UIHit outside = scene.Tree.HitTest({ 900.0f, 500.0f });
	CHECK(outside.HoverId == 0);
	CHECK(!outside.OverBlocking);
	CHECK(!outside.Modal);
}

TEST_CASE("UI hit-test gives overlapping roots to the higher layer, then the later declared")
{
	UITestScene layered;
	layered.OpenRoot("low", { 100.0f, 100.0f }, { 200.0f, 200.0f }, 0);
	const uint64_t low = layered.Widget("low", { 200.0f, 200.0f });
	layered.CloseRoot();
	layered.OpenRoot("high", { 150.0f, 150.0f }, { 200.0f, 200.0f }, 5);
	const uint64_t high = layered.Widget("high", { 200.0f, 200.0f });
	layered.CloseRoot();
	layered.Tree.CloseAll();
	layered.Tree.Layout(k_Measure);
	CHECK(layered.Tree.HitTest({ 200.0f, 200.0f }).HoverId == high);
	CHECK(layered.Tree.HitTest({ 120.0f, 120.0f }).HoverId == low);

	UITestScene reversed;
	reversed.OpenRoot("high", { 150.0f, 150.0f }, { 200.0f, 200.0f }, 5);
	const uint64_t top = reversed.Widget("high", { 200.0f, 200.0f });
	reversed.CloseRoot();
	reversed.OpenRoot("low", { 100.0f, 100.0f }, { 200.0f, 200.0f }, 0);
	reversed.Widget("low", { 200.0f, 200.0f });
	reversed.CloseRoot();
	reversed.Tree.CloseAll();
	reversed.Tree.Layout(k_Measure);
	CHECK(reversed.Tree.HitTest({ 200.0f, 200.0f }).HoverId == top);

	UITestScene sameLayer;
	sameLayer.OpenRoot("first", { 100.0f, 100.0f }, { 200.0f, 200.0f });
	sameLayer.Widget("first", { 200.0f, 200.0f });
	sameLayer.CloseRoot();
	sameLayer.OpenRoot("second", { 100.0f, 100.0f }, { 200.0f, 200.0f });
	const uint64_t second = sameLayer.Widget("second", { 200.0f, 200.0f });
	sameLayer.CloseRoot();
	sameLayer.Tree.CloseAll();
	sameLayer.Tree.Layout(k_Measure);
	CHECK(sameLayer.Tree.HitTest({ 200.0f, 200.0f }).HoverId == second);
}

TEST_CASE("UI hit-test: a later interactive child wins over its parent")
{
	UITestScene scene;
	scene.OpenRoot("r", { 0.0f, 0.0f }, { 400.0f, 400.0f });
	UIWidgetInfo parentInfo;
	parentInfo.Id = scene.Tree.MakeId("parent");
	parentInfo.Interactive = true;
	REQUIRE(scene.Tree.BeginBox(UIBoxParams().SetSize(UISize::Fixed(200.0f), UISize::Fixed(200.0f)).SetPadding(20.0f), &parentInfo));
	const uint64_t child = scene.Widget("child", { 60.0f, 60.0f });
	REQUIRE(scene.Tree.EndBox());
	scene.CloseRoot();
	scene.Tree.Layout(k_Measure);

	CHECK(scene.Tree.HitTest({ 40.0f, 40.0f }).HoverId == child);
	CHECK(scene.Tree.HitTest({ 5.0f, 5.0f }).HoverId == parentInfo.Id);
}

TEST_CASE("UI hit-test: a blocking root hides what is under it, a non-blocking one does not")
{
	UITestScene blocking;
	blocking.OpenRoot("under", { 100.0f, 100.0f }, { 200.0f, 200.0f }, 0);
	blocking.Widget("under", { 200.0f, 200.0f });
	blocking.CloseRoot();
	blocking.OpenRoot("panel", { 100.0f, 100.0f }, { 200.0f, 200.0f }, 1);
	blocking.Widget("panel", { 200.0f, 200.0f }, false, true);
	blocking.CloseRoot();
	blocking.Tree.CloseAll();
	blocking.Tree.Layout(k_Measure);
	const UIHit blocked = blocking.Tree.HitTest({ 200.0f, 200.0f });
	CHECK(blocked.HoverId == 0);
	CHECK(blocked.OverBlocking);

	UITestScene passing;
	passing.OpenRoot("under", { 100.0f, 100.0f }, { 200.0f, 200.0f }, 0);
	const uint64_t under = passing.Widget("under", { 200.0f, 200.0f });
	passing.CloseRoot();
	passing.OpenRoot("hud", { 100.0f, 100.0f }, { 200.0f, 200.0f }, 1, false);
	passing.Widget("hud", { 200.0f, 200.0f }, false, true);
	passing.CloseRoot();
	passing.Tree.CloseAll();
	passing.Tree.Layout(k_Measure);
	const UIHit passed = passing.Tree.HitTest({ 200.0f, 200.0f });
	CHECK(passed.HoverId == under);
	CHECK(passed.OverBlocking);
}

TEST_CASE("UI hit-test: a non-blocking root takes hover but is never over the UI")
{
	UITestScene scene;
	scene.OpenRoot("hud", { 100.0f, 100.0f }, { 200.0f, 200.0f }, 0, false);
	const uint64_t widget = scene.Widget("w", { 200.0f, 200.0f });
	scene.CloseRoot();
	const UIHit hit = scene.Hit({ 200.0f, 200.0f });
	CHECK(hit.HoverId == widget);
	CHECK(!hit.OverBlocking);
}

TEST_CASE("UI hit-test: a bare layout box of a blocking root takes nothing")
{
	UITestScene scene;
	scene.OpenRoot("r", { 100.0f, 100.0f }, { 200.0f, 200.0f });
	scene.CloseRoot();
	const UIHit hit = scene.Hit({ 150.0f, 150.0f });
	CHECK(!hit.OverBlocking);
	CHECK(hit.HoverId == 0);
}

TEST_CASE("UI hit-test: a modal root hides everything below it")
{
	UITestScene scene;
	scene.OpenRoot("game", { 0.0f, 0.0f }, { 400.0f, 400.0f }, 0);
	scene.Widget("game", { 400.0f, 400.0f });
	scene.CloseRoot();
	scene.OpenRoot("dialog", { 600.0f, 100.0f }, { 100.0f, 100.0f }, 3, true, true);
	const uint64_t ok = scene.Widget("ok", { 100.0f, 100.0f });
	scene.CloseRoot();
	scene.OpenRoot("toast", { 0.0f, 500.0f }, { 100.0f, 50.0f }, 9, false);
	const uint64_t toast = scene.Widget("toast", { 100.0f, 50.0f });
	scene.CloseRoot();
	scene.Tree.CloseAll();
	scene.Tree.Layout(k_Measure);

	CHECK(scene.Tree.HasModal());
	const UIHit below = scene.Tree.HitTest({ 100.0f, 100.0f });
	CHECK(below.Modal);
	CHECK(below.OverBlocking);
	CHECK(below.HoverId == 0);

	CHECK(scene.Tree.HitTest({ 650.0f, 150.0f }).HoverId == ok);
	const UIHit above = scene.Tree.HitTest({ 50.0f, 520.0f });
	CHECK(above.HoverId == toast);
	CHECK(above.Modal);
	CHECK(scene.Tree.HitTest({ 900.0f, 20.0f }).OverBlocking);

	UITestScene plain;
	plain.OpenRoot("r", { 0.0f, 0.0f }, { 10.0f, 10.0f });
	plain.CloseRoot();
	plain.Tree.CloseAll();
	CHECK(!plain.Tree.HasModal());
}

TEST_CASE("UI hit-test ignores the part of an element a clipping parent hides")
{
	UITestScene scene;
	scene.OpenRoot("r", { 0.0f, 0.0f }, { 400.0f, 400.0f });
	UIWidgetInfo areaInfo;
	areaInfo.Id = scene.Tree.MakeId("area");
	areaInfo.Scroll = true;
	REQUIRE(scene.Tree.BeginBox(UIBoxParams().SetSize(UISize::Fixed(100.0f), UISize::Fixed(100.0f)), &areaInfo));
	const uint64_t tall = scene.Widget("tall", { 100.0f, 300.0f });
	REQUIRE(scene.Tree.EndBox());
	scene.CloseRoot();
	scene.Tree.Layout(k_Measure);

	const UIHit inside = scene.Tree.HitTest({ 50.0f, 50.0f });
	CHECK(inside.HoverId == tall);
	CHECK(inside.ScrollId == areaInfo.Id);
	const UIHit clipped = scene.Tree.HitTest({ 50.0f, 200.0f });
	CHECK(clipped.HoverId == 0);
	CHECK(clipped.ScrollId == 0);
}

TEST_CASE("UI scroll areas report their content size and move their children")
{
	UITestScene scene;
	scene.OpenRoot("r", { 0.0f, 0.0f }, { 400.0f, 400.0f });
	UIWidgetInfo areaInfo;
	areaInfo.Id = scene.Tree.MakeId("area");
	areaInfo.Scroll = true;
	areaInfo.ScrollOffset = { 0.0f, 30.0f };
	REQUIRE(scene.Tree.BeginBox(UIBoxParams().SetSize(UISize::Fixed(100.0f), UISize::Fixed(100.0f)).SetGap(10.0f).SetPadding(5.0f), &areaInfo));
	scene.Widget("one", { 80.0f, 60.0f });
	scene.Widget("two", { 80.0f, 60.0f });
	REQUIRE(scene.Tree.EndBox());
	scene.CloseRoot();
	scene.Tree.Layout(k_Measure);

	const UIElement* area = scene.Tree.FindById(areaInfo.Id);
	REQUIRE(area != nullptr);
	CHECK(area->ContentSize.y == doctest::Approx(5.0f + 60.0f + 10.0f + 60.0f + 5.0f));
	CHECK(area->ClipChildren);
	const UIElement* one = scene.Tree.FindByName("one");
	REQUIRE(one != nullptr);
	CHECK(one->Rect.Min.y == doctest::Approx(5.0f - 30.0f));
	CHECK(one->Clip.Min.y == doctest::Approx(0.0f));
	CHECK(one->Clip.Max.y == doctest::Approx(100.0f));
}
