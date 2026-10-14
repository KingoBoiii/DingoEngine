#include <doctest/doctest.h>

#include "DingoEngine/UI/DingoUI/DingoUILayout.h"
#include "DingoEngine/UI/DingoUI/DingoUIWorld.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace Dingo;
using namespace Dingo::Internal;

namespace
{
	const glm::vec2 k_Viewport = { 1280.0f, 720.0f };

	glm::mat4 TestViewProjection()
	{
		const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::mat4 projection = glm::perspective(glm::radians(60.0f), k_Viewport.x / k_Viewport.y, 0.1f, 100.0f);
		return projection * view;
	}
}

TEST_SUITE("DingoUIWorld")
{
	TEST_CASE("a point on the view axis lands at the centre, rounded")
	{
		const UIWorldPlacement placement = ComputeWorldPlacement(TestViewProjection(), glm::vec3(0.0f), k_Viewport, UIWorldRootParams());
		CHECK(placement.Visible);
		CHECK(placement.Point.x == doctest::Approx(640.0f));
		CHECK(placement.Point.y == doctest::Approx(360.0f));
		CHECK(placement.Depth == doctest::Approx(10.0f));
		CHECK(placement.Scale == 1.0f);
		CHECK(placement.Alpha == 1.0f);
	}

	TEST_CASE("behind the camera is culled")
	{
		CHECK_FALSE(ComputeWorldPlacement(TestViewProjection(), glm::vec3(0.0f, 0.0f, 20.0f), k_Viewport, UIWorldRootParams()).Visible);
	}

	TEST_CASE("off screen is culled past the margin only")
	{
		const glm::mat4 viewProjection = TestViewProjection();
		const glm::vec3 far(40.0f, 0.0f, 0.0f);
		CHECK_FALSE(ComputeWorldPlacement(viewProjection, far, k_Viewport, UIWorldRootParams()).Visible);
		CHECK(ComputeWorldPlacement(viewProjection, far, k_Viewport, UIWorldRootParams().SetCullMargin(100000.0f)).Visible);
		const glm::vec3 edge(12.0f, 0.0f, 0.0f);
		CHECK(ComputeWorldPlacement(viewProjection, edge, k_Viewport, UIWorldRootParams().SetCullMargin(0.0f)).Visible == false);
		CHECK(ComputeWorldPlacement(viewProjection, edge, k_Viewport, UIWorldRootParams().SetCullMargin(200.0f)).Visible);
	}

	TEST_CASE("distance scale clamps")
	{
		const glm::mat4 viewProjection = TestViewProjection();
		UIWorldRootParams params = UIWorldRootParams().SetDistanceScale(20.0f, 0.5f, 1.5f);
		CHECK(ComputeWorldPlacement(viewProjection, glm::vec3(0.0f), k_Viewport, params).Scale == doctest::Approx(1.5f));
		CHECK(ComputeWorldPlacement(viewProjection, glm::vec3(0.0f, 0.0f, -30.0f), k_Viewport, params).Scale == doctest::Approx(0.5f));
		params = UIWorldRootParams().SetDistanceScale(20.0f, 0.1f, 10.0f);
		CHECK(ComputeWorldPlacement(viewProjection, glm::vec3(0.0f), k_Viewport, params).Scale == doctest::Approx(2.0f));
	}

	TEST_CASE("max distance culls and fades")
	{
		const glm::mat4 viewProjection = TestViewProjection();
		const UIWorldRootParams params = UIWorldRootParams().SetMaxDistance(20.0f, 10.0f);
		CHECK(ComputeWorldPlacement(viewProjection, glm::vec3(0.0f), k_Viewport, params).Alpha == 1.0f);
		CHECK(ComputeWorldPlacement(viewProjection, glm::vec3(0.0f, 0.0f, -5.0f), k_Viewport, params).Alpha == doctest::Approx(0.5f));
		CHECK_FALSE(ComputeWorldPlacement(viewProjection, glm::vec3(0.0f, 0.0f, -11.0f), k_Viewport, params).Visible);
	}

	TEST_CASE("world roots sort below screen roots, far first")
	{
		UILayoutTree tree;
		tree.Begin(k_Viewport);
		UIRootParams screen;
		screen.Layer = -5;
		tree.BeginRoot("screen", screen);
		tree.EndRoot();
		tree.BeginWorldRoot("near", UIWorldRootParams(), { 100.0f, 100.0f }, 5.0f, 1.0f, 1.0f);
		tree.EndRoot();
		tree.BeginWorldRoot("far", UIWorldRootParams(), { 100.0f, 100.0f }, 50.0f, 1.0f, 1.0f);
		tree.EndRoot();

		std::vector<int32_t> order;
		tree.GetRootOrder(order);
		REQUIRE(order.size() == 3);
		CHECK(tree.GetElements()[order[0]].NameHash == UILayoutTree::HashName("far"));
		CHECK(tree.GetElements()[order[1]].NameHash == UILayoutTree::HashName("near"));
		CHECK(tree.GetElements()[order[2]].NameHash == UILayoutTree::HashName("screen"));
	}

	TEST_CASE("pivot places the root on the point")
	{
		UILayoutTree tree;
		tree.Begin(k_Viewport);
		UIWorldRootParams params;
		params.Layout.Width = UISize::Fixed(100.0f);
		params.Layout.Height = UISize::Fixed(20.0f);
		tree.BeginWorldRoot("bar", params, { 400.0f, 300.0f }, 5.0f, 1.0f, 1.0f);
		tree.EndRoot();
		const UITextMeasureFn measure = [](const Font*, std::string_view, float, float) { return glm::vec2(0.0f); };
		tree.Layout(measure);
		const UIElement* bar = tree.FindByName("bar");
		REQUIRE(bar);
		CHECK(bar->Rect.Min.x == doctest::Approx(350.0f));
		CHECK(bar->Rect.Max.x == doctest::Approx(450.0f));
		CHECK(bar->Rect.Max.y == doctest::Approx(300.0f));
		CHECK_FALSE(tree.HitTest({ 400.0f, 290.0f }).OverBlocking);
	}
}
