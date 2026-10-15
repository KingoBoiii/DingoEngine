#include <doctest/doctest.h>

#include "DingoEngine/UI/DingoUI/DingoUILayout.h"
#include "DingoEngine/UI/DingoUI/DingoUINav.h"
#include "DingoEngine/Core/InputBackend.h"

#include <vector>

using namespace Dingo;
using namespace Dingo::Internal;

namespace
{
	UINavCandidate At(uint64_t id, float x, float y, float width = 100.0f, float height = 40.0f)
	{
		UINavCandidate candidate;
		candidate.Id = id;
		candidate.Rect.Min = { x, y };
		candidate.Rect.Max = { x + width, y + height };
		return candidate;
	}

	// Ids 1..6 in rows of three: 1 2 3 / 4 5 6.
	std::vector<UINavCandidate> Grid()
	{
		std::vector<UINavCandidate> grid;
		for (uint64_t row = 0; row < 2; ++row)
		{
			for (uint64_t column = 0; column < 3; ++column)
				grid.push_back(At(row * 3 + column + 1, 10.0f + column * 110.0f, 10.0f + row * 50.0f));
		}
		return grid;
	}
}

TEST_CASE("UINav: a grid moves to the neighbour in each direction")
{
	const std::vector<UINavCandidate> grid = Grid();
	CHECK(FindNavTarget(grid, 0, UINavDirection::Right) == 1);
	CHECK(FindNavTarget(grid, 1, UINavDirection::Right) == 2);
	CHECK(FindNavTarget(grid, 0, UINavDirection::Down) == 3);
	CHECK(FindNavTarget(grid, 4, UINavDirection::Up) == 1);
	CHECK(FindNavTarget(grid, 4, UINavDirection::Left) == 3);
	CHECK(FindNavTarget(grid, 2, UINavDirection::Down) == 5);
}

TEST_CASE("UINav: the edges of a grid stop")
{
	const std::vector<UINavCandidate> grid = Grid();
	CHECK(FindNavTarget(grid, 0, UINavDirection::Left) == -1);
	CHECK(FindNavTarget(grid, 0, UINavDirection::Up) == -1);
	CHECK(FindNavTarget(grid, 2, UINavDirection::Right) == -1);
	CHECK(FindNavTarget(grid, 5, UINavDirection::Down) == -1);
}

TEST_CASE("UINav: an out-of-range origin has no target")
{
	const std::vector<UINavCandidate> grid = Grid();
	CHECK(FindNavTarget(grid, -1, UINavDirection::Down) == -1);
	CHECK(FindNavTarget(grid, 6, UINavDirection::Down) == -1);
	CHECK(FindNavTarget({}, 0, UINavDirection::Down) == -1);
}

TEST_CASE("UINav: down from a wide item lands on the item beneath its centre")
{
	const std::vector<UINavCandidate> items = { At(1, 0.0f, 0.0f, 320.0f), At(2, 0.0f, 60.0f, 100.0f), At(3, 110.0f, 60.0f, 100.0f), At(4, 220.0f, 60.0f, 100.0f) };
	CHECK(FindNavTarget(items, 0, UINavDirection::Down) == 2);
	CHECK(FindNavTarget(items, 1, UINavDirection::Up) == 0);
	CHECK(FindNavTarget(items, 3, UINavDirection::Up) == 0);
}

TEST_CASE("UINav: a column with a side item prefers the aligned one")
{
	const std::vector<UINavCandidate> items = { At(1, 0.0f, 0.0f), At(2, 300.0f, 55.0f), At(3, 0.0f, 110.0f) };
	CHECK(FindNavTarget(items, 0, UINavDirection::Down) == 2);
	CHECK(FindNavTarget(items, 0, UINavDirection::Right) == 1);
}

TEST_CASE("UINav: an item level with the origin is not below it")
{
	const std::vector<UINavCandidate> items = { At(1, 0.0f, 0.0f), At(2, 200.0f, 0.0f) };
	CHECK(FindNavTarget(items, 0, UINavDirection::Down) == -1);
	CHECK(FindNavTarget(items, 0, UINavDirection::Right) == 1);
}

TEST_CASE("UINav: a tie goes to the earlier declaration")
{
	const std::vector<UINavCandidate> items = { At(1, 100.0f, 0.0f), At(2, 0.0f, 60.0f), At(3, 200.0f, 60.0f) };
	CHECK(FindNavTarget(items, 0, UINavDirection::Down) == 1);
}

TEST_CASE("UINav: the nearest item to a point")
{
	const std::vector<UINavCandidate> grid = Grid();
	CHECK(FindNearestNav(grid, { 15.0f, 15.0f }) == 0);
	CHECK(FindNearestNav(grid, { 250.0f, 80.0f }) == 5);
	CHECK(FindNearestNav({}, { 0.0f, 0.0f }) == -1);
}

TEST_CASE("UINav: tab order wraps in both directions")
{
	CHECK(StepNavOrder(3, 0, true) == 1);
	CHECK(StepNavOrder(3, 2, true) == 0);
	CHECK(StepNavOrder(3, 0, false) == 2);
	CHECK(StepNavOrder(3, -1, true) == 0);
	CHECK(StepNavOrder(3, -1, false) == 2);
	CHECK(StepNavOrder(0, 0, true) == -1);
}

TEST_CASE("UINav: a held direction moves at once, then repeats after the delay")
{
	UINavRepeater repeater;
	CHECK(repeater.Step(true, 0.016f, 0.4f, 0.1f));
	int moves = 0;
	for (int frame = 0; frame < 60; ++frame)
		moves += repeater.Step(true, 0.016f, 0.4f, 0.1f) ? 1 : 0;
	CHECK(moves >= 6);
	CHECK(moves <= 9);
}

TEST_CASE("UINav: no repeat before the delay, and releasing resets it")
{
	UINavRepeater repeater;
	CHECK(repeater.Step(true, 0.1f, 0.4f, 0.1f));
	CHECK_FALSE(repeater.Step(true, 0.1f, 0.4f, 0.1f));
	CHECK_FALSE(repeater.Step(true, 0.1f, 0.4f, 0.1f));
	CHECK_FALSE(repeater.Step(false, 0.1f, 0.4f, 0.1f));
	CHECK(repeater.Step(true, 0.1f, 0.4f, 0.1f));
}

TEST_CASE("UINav: the tree carries the focus flags of a widget and the navigable flag of a root")
{
	UILayoutTree tree;
	tree.Begin({ 400.0f, 300.0f });
	REQUIRE(tree.BeginRoot("menu", UIRootParams().SetNavigable(false)));
	UIWidgetInfo widget;
	widget.Id = tree.MakeId("Play");
	widget.Interactive = true;
	widget.Focusable = true;
	widget.DefaultFocus = true;
	widget.AdjustsHorizontally = true;
	REQUIRE(tree.BeginBox(UIBoxParams(), &widget));
	tree.EndBox();
	tree.EndRoot();

	const std::vector<UIElement>& elements = tree.GetElements();
	REQUIRE(elements.size() == 2);
	CHECK_FALSE(elements[0].Navigable);
	CHECK(elements[1].Focusable);
	CHECK(elements[1].DefaultFocus);
	CHECK(elements[1].AdjustsHorizontally);

	std::vector<int32_t> order;
	tree.GetRootOrder(order);
	REQUIRE(order.size() == 1);
	CHECK(order[0] == 0);
}

TEST_CASE("UINav: a direction consumes the held direction key and leaves the other keys")
{
	InputBackend::ClearConsumed();
	const UINavParams params;
	InputBackend::SimulateKey(Key::Left, true);
	InputBackend::SimulateKey(Key::Up, true);
	UINavInput fired;
	ReadNavInput(fired, true, params, 0);
	fired.Direction[static_cast<size_t>(UINavDirection::Up)] = false;
	ConsumeNavInput(fired, true, params);
	CHECK_FALSE(Input::IsKeyDown(Key::Left));
	CHECK(Input::IsKeyDown(Key::Up));

	InputBackend::SimulateKey(Key::Left, false);
	InputBackend::SimulateKey(Key::Up, false);
	InputBackend::ClearConsumed();
}
