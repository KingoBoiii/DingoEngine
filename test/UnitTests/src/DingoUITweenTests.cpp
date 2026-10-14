#include <doctest/doctest.h>

#include "DingoEngine/UI/DingoUI/DingoUITween.h"

#include <cmath>

using namespace Dingo;
using namespace Dingo::Internal;

namespace
{
	constexpr UIEase k_Eases[] = { UIEase::Linear, UIEase::QuadIn, UIEase::QuadOut, UIEase::QuadInOut, UIEase::CubicIn,
		UIEase::CubicOut, UIEase::CubicInOut, UIEase::BackOut, UIEase::ElasticOut };

	UITransition FadeOnly(float duration)
	{
		return UITransition::Fade(duration).SetEase(UIEase::Linear);
	}
}

TEST_CASE("Easing curves start at 0, end at 1 and clamp outside")
{
	for (const UIEase ease : k_Eases)
	{
		CHECK(EvaluateEase(ease, 0.0f) == 0.0f);
		CHECK(EvaluateEase(ease, 1.0f) == 1.0f);
		CHECK(EvaluateEase(ease, -3.0f) == 0.0f);
		CHECK(EvaluateEase(ease, 7.0f) == 1.0f);
	}
}

TEST_CASE("Easing curves have the expected midpoints and shape")
{
	CHECK(EvaluateEase(UIEase::Linear, 0.25f) == doctest::Approx(0.25f));
	CHECK(EvaluateEase(UIEase::QuadIn, 0.5f) == doctest::Approx(0.25f));
	CHECK(EvaluateEase(UIEase::QuadOut, 0.5f) == doctest::Approx(0.75f));
	CHECK(EvaluateEase(UIEase::QuadInOut, 0.5f) == doctest::Approx(0.5f));
	CHECK(EvaluateEase(UIEase::QuadInOut, 0.25f) == doctest::Approx(0.125f));
	CHECK(EvaluateEase(UIEase::CubicIn, 0.5f) == doctest::Approx(0.125f));
	CHECK(EvaluateEase(UIEase::CubicOut, 0.5f) == doctest::Approx(0.875f));
	CHECK(EvaluateEase(UIEase::CubicInOut, 0.5f) == doctest::Approx(0.5f));
	CHECK(EvaluateEase(UIEase::CubicInOut, 0.25f) == doctest::Approx(0.0625f));

	CHECK(EvaluateEase(UIEase::BackOut, 0.7f) > 1.0f);
	CHECK(EvaluateEase(UIEase::ElasticOut, 0.2f) > 1.0f);
	for (const UIEase ease : { UIEase::QuadIn, UIEase::QuadOut, UIEase::CubicIn, UIEase::CubicOut, UIEase::QuadInOut, UIEase::CubicInOut })
	{
		float previous = 0.0f;
		for (int i = 1; i <= 20; ++i)
		{
			const float value = EvaluateEase(ease, static_cast<float>(i) / 20.0f);
			CHECK(value >= previous);
			CHECK(value <= 1.0f);
			previous = value;
		}
	}
}

TEST_CASE("StepProgress moves by dt over duration and clamps")
{
	CHECK(StepProgress(0.0f, 0.05f, 0.2f, true) == doctest::Approx(0.25f));
	CHECK(StepProgress(0.9f, 0.05f, 0.2f, true) == 1.0f);
	CHECK(StepProgress(0.1f, 0.05f, 0.2f, false) == 0.0f);
	CHECK(StepProgress(0.5f, 0.0f, 0.2f, true) == 0.5f);
	CHECK(StepProgress(0.5f, -1.0f, 0.2f, true) == 0.5f);
	CHECK(StepProgress(0.3f, 0.01f, 0.0f, true) == 1.0f);
	CHECK(StepProgress(0.3f, 0.01f, 0.0f, false) == 0.0f);
}

TEST_CASE("A tween snaps on its first call, then moves to a new target over its duration")
{
	UITweenMap tweens;
	uint64_t frame = 0;
	const auto step = [&](float target, float duration = 1.0f, UIEase ease = UIEase::Linear)
	{
		return tweens.Update(7, glm::vec4(target, 0.0f, 0.0f, 0.0f), 0.25f, duration, ease, frame++).x;
	};

	CHECK(step(0.0f) == 0.0f);
	CHECK(step(0.0f) == 0.0f);
	CHECK(step(1.0f) == 0.0f);
	CHECK(step(1.0f) == doctest::Approx(0.25f).epsilon(1e-6));
	CHECK(step(1.0f) == doctest::Approx(0.5f).epsilon(1e-6));
	CHECK(step(1.0f) == doctest::Approx(0.75f).epsilon(1e-6));
	CHECK(step(1.0f) == 1.0f);
	CHECK(step(1.0f) == 1.0f);
}

TEST_CASE("A tween redirected midway restarts from where it was")
{
	UITweenMap tweens;
	uint64_t frame = 0;
	const auto step = [&](float target) { return tweens.Update(1, glm::vec4(target, 0.0f, 0.0f, 0.0f), 0.25f, 1.0f, UIEase::Linear, frame++).x; };
	step(0.0f);
	step(1.0f);
	step(1.0f);
	CHECK(step(1.0f) == doctest::Approx(0.5f).epsilon(1e-6));
	CHECK(step(0.0f) == doctest::Approx(0.5f).epsilon(1e-6));
	CHECK(step(0.0f) == doctest::Approx(0.375f).epsilon(1e-6));
}

TEST_CASE("A tween follows its ease and may overshoot")
{
	UITweenMap tweens;
	uint64_t frame = 0;
	const auto step = [&](float target) { return tweens.Update(2, glm::vec4(target, 0.0f, 0.0f, 0.0f), 0.25f, 1.0f, UIEase::QuadIn, frame++).x; };
	step(0.0f);
	step(1.0f);
	CHECK(step(1.0f) == doctest::Approx(0.0625f).epsilon(1e-6));
	CHECK(step(1.0f) == doctest::Approx(0.25f).epsilon(1e-6));

	UITweenMap back;
	frame = 0;
	const auto stepBack = [&](float target) { return back.Update(3, glm::vec4(target, 0.0f, 0.0f, 0.0f), 0.1f, 1.0f, UIEase::BackOut, frame++).x; };
	stepBack(0.0f);
	float peak = 0.0f;
	for (int i = 0; i < 12; ++i)
		peak = (std::max)(peak, stepBack(1.0f));
	CHECK(peak > 1.0f);
	CHECK(stepBack(1.0f) == 1.0f);
}

TEST_CASE("A zero-duration tween jumps, a second call in a frame does not advance it, and unseen tweens are dropped")
{
	UITweenMap tweens;
	tweens.Update(1, glm::vec4(0.0f), 0.1f, 0.0f, UIEase::Linear, 0);
	CHECK(tweens.Update(1, glm::vec4(5.0f, 0.0f, 0.0f, 0.0f), 0.1f, 0.0f, UIEase::Linear, 1).x == 5.0f);

	tweens.Update(2, glm::vec4(0.0f), 0.5f, 1.0f, UIEase::Linear, 2);
	tweens.Update(2, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), 0.5f, 1.0f, UIEase::Linear, 3);
	const float once = tweens.Update(2, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), 0.5f, 1.0f, UIEase::Linear, 4).x;
	CHECK(tweens.Update(2, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), 0.5f, 1.0f, UIEase::Linear, 4).x == once);

	CHECK(tweens.Size() == 2);
	CHECK(tweens.Evict(5) == 1);
	CHECK(tweens.Evict(6) == 1);
	CHECK(tweens.Size() == 0);
}

TEST_CASE("A colour tween moves every channel")
{
	UITweenMap tweens;
	tweens.Update(1, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), 0.5f, 1.0f, UIEase::Linear, 0);
	tweens.Update(1, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f), 0.5f, 1.0f, UIEase::Linear, 1);
	const glm::vec4 halfway = tweens.Update(1, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f), 0.5f, 1.0f, UIEase::Linear, 2);
	CHECK(halfway.x == doctest::Approx(0.5f));
	CHECK(halfway.y == doctest::Approx(0.25f));
	CHECK(halfway.w == doctest::Approx(1.0f));
}

TEST_CASE("A root with a transition appears hidden and fades in over its duration")
{
	UIRootTransitions roots;
	const float dt = 0.05f;
	uint64_t frame = 0;
	const float expected[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.0f };
	for (const float alpha : expected)
	{
		UIRootTransitions::Entry* entry = roots.Declare(9, FadeOnly(0.2f), 0, frame++, dt);
		REQUIRE(entry != nullptr);
		const UIRootVisual visual = UIRootTransitions::VisualOf(*entry);
		CHECK(visual.Alpha == doctest::Approx(alpha).epsilon(1e-5));
		CHECK(visual.Scale == 1.0f);
	}
	CHECK(UIRootTransitions::VisualOf(*roots.Declare(9, FadeOnly(0.2f), 0, frame, dt)).IsIdentity());
}

TEST_CASE("A root with no transition, or a zero duration, is not held back")
{
	UIRootTransitions roots;
	CHECK(roots.Declare(1, UITransition{}, 0, 0, 0.05f) == nullptr);
	CHECK(roots.Declare(1, UITransition::Fade(0.0f), 0, 0, 0.05f) == nullptr);
	CHECK(roots.Size() == 0);

	UITransition exitOnly = UITransition::Fade(0.0f).SetExitDuration(0.2f);
	UIRootTransitions::Entry* entry = roots.Declare(2, exitOnly, 0, 0, 0.05f);
	REQUIRE(entry != nullptr);
	CHECK(entry->Progress == 1.0f);
}

TEST_CASE("A root that is no longer declared plays out its kept commands, then goes")
{
	UIRootTransitions roots;
	std::vector<UIRootTransitions::Entry*> exiting;
	const float dt = 0.05f;

	for (uint64_t frame = 0; frame < 6; ++frame)
	{
		UIRootTransitions::Entry* entry = roots.Declare(4, FadeOnly(0.2f), 3, frame, dt);
		entry->Kept.AddRect({ { 0.0f, 0.0f }, { 10.0f, 10.0f } }, { 1.0f, 1.0f, 1.0f, 1.0f });
		roots.AdvanceExiting(frame, dt, exiting);
		CHECK(exiting.empty());
	}

	const float expected[] = { 0.75f, 0.5f, 0.25f };
	uint64_t frame = 6;
	for (const float alpha : expected)
	{
		roots.AdvanceExiting(frame++, dt, exiting);
		REQUIRE(exiting.size() == 1);
		CHECK(exiting[0]->Layer == 3);
		CHECK(UIRootTransitions::VisualOf(*exiting[0]).Alpha == doctest::Approx(alpha).epsilon(1e-5));
	}
	roots.AdvanceExiting(frame, dt, exiting);
	CHECK(exiting.empty());
	CHECK(roots.Size() == 0);
}

TEST_CASE("An exit can be shorter than the entry, be skipped, and be reversed by declaring the root again")
{
	UIRootTransitions roots;
	std::vector<UIRootTransitions::Entry*> exiting;
	const float dt = 0.05f;

	const UITransition quick = FadeOnly(0.4f).SetExitDuration(0.1f);
	for (uint64_t frame = 0; frame < 10; ++frame)
		roots.Declare(1, quick, 0, frame, dt)->Kept.AddRect({ { 0.0f, 0.0f }, { 1.0f, 1.0f } }, glm::vec4(1.0f));
	roots.AdvanceExiting(10, dt, exiting);
	REQUIRE(exiting.size() == 1);
	CHECK(exiting[0]->Progress == doctest::Approx(0.5f));
	roots.Declare(1, quick, 0, 11, dt);
	roots.AdvanceExiting(11, dt, exiting);
	CHECK(exiting.empty());
	CHECK(roots.Size() == 1);
	CHECK(roots.Declare(1, quick, 0, 12, dt)->Progress == doctest::Approx(0.5f + 0.25f));

	UIRootTransitions instant;
	const UITransition none = FadeOnly(0.4f).SetExitDuration(0.0f);
	instant.Declare(2, none, 0, 0, dt)->Kept.AddRect({ { 0.0f, 0.0f }, { 1.0f, 1.0f } }, glm::vec4(1.0f));
	instant.AdvanceExiting(1, dt, exiting);
	CHECK(exiting.empty());
	CHECK(instant.Size() == 0);
}

TEST_CASE("Exiting roots are listed by layer and a root without kept commands is dropped")
{
	UIRootTransitions roots;
	std::vector<UIRootTransitions::Entry*> exiting;
	for (uint64_t frame = 0; frame < 4; ++frame)
	{
		roots.Declare(1, FadeOnly(1.0f), 5, frame, 0.1f)->Kept.AddRect({ { 0.0f, 0.0f }, { 1.0f, 1.0f } }, glm::vec4(1.0f));
		roots.Declare(2, FadeOnly(1.0f), 2, frame, 0.1f)->Kept.AddRect({ { 0.0f, 0.0f }, { 1.0f, 1.0f } }, glm::vec4(1.0f));
		roots.Declare(3, FadeOnly(1.0f), 4, frame, 0.1f);
	}
	roots.AdvanceExiting(4, 0.1f, exiting);
	REQUIRE(exiting.size() == 2);
	CHECK(exiting[0]->Layer == 2);
	CHECK(exiting[1]->Layer == 5);
	CHECK(roots.Size() == 2);
}

TEST_CASE("Slide and scale visuals interpolate to the identity")
{
	UIRootTransitions roots;
	const UITransition move = UITransition::Slide({ 0.0f, 40.0f }, 1.0f).WithScale(0.5f).SetEase(UIEase::Linear);
	UIRootTransitions::Entry* entry = roots.Declare(1, move, 0, 0, 0.5f);
	UIRootVisual visual = UIRootTransitions::VisualOf(*entry);
	CHECK(visual.Alpha == 1.0f);
	CHECK(visual.Offset.y == doctest::Approx(40.0f));
	CHECK(visual.Scale == doctest::Approx(0.5f));
	entry = roots.Declare(1, move, 0, 1, 0.5f);
	visual = UIRootTransitions::VisualOf(*entry);
	CHECK(visual.Offset.y == doctest::Approx(20.0f));
	CHECK(visual.Scale == doctest::Approx(0.75f));
	entry = roots.Declare(1, move, 0, 2, 0.5f);
	CHECK(UIRootTransitions::VisualOf(*entry).IsIdentity());
}

TEST_CASE("AppendTransformed moves, scales and fades a copy of the commands")
{
	UIDrawList source;
	source.AddRect({ { 10.0f, 10.0f }, { 30.0f, 20.0f } }, { 1.0f, 0.0f, 0.0f, 1.0f }, 4.0f, 2.0f, { 0.0f, 1.0f, 0.0f, 0.5f }, { { 0.0f, 0.0f }, { 25.0f, 100.0f } });

	UIDrawList list;
	list.AddRect({ { 0.0f, 0.0f }, { 1.0f, 1.0f } }, glm::vec4(1.0f));
	list.AppendTransformed(source, 0.5f, { 5.0f, 7.0f }, 2.0f, { 10.0f, 10.0f });
	REQUIRE(list.GetCommandCount() == 2);

	UIGeometry geometry;
	list.Build(geometry);
	REQUIRE(geometry.Vertices.size() == 8);
	const UIVertex& topLeft = geometry.Vertices[4];
	const UIVertex& bottomRight = geometry.Vertices[6];
	CHECK(topLeft.Position.x == doctest::Approx(15.0f));
	CHECK(topLeft.Position.y == doctest::Approx(17.0f));
	CHECK(bottomRight.Position.x == doctest::Approx(55.0f));
	CHECK(bottomRight.Position.y == doctest::Approx(37.0f));
	CHECK(topLeft.Color.a == doctest::Approx(0.5f));
	CHECK(topLeft.BorderColor.a == doctest::Approx(0.25f));
	CHECK(topLeft.Radii.x == doctest::Approx(8.0f));
	CHECK(topLeft.Params.x == doctest::Approx(4.0f));
	CHECK(topLeft.Clip.z == doctest::Approx(45.0f));
}

TEST_CASE("AppendTransformed keeps the unbounded clip and a text's bytes")
{
	UIDrawList plain;
	plain.AddRect({ { 0.0f, 0.0f }, { 4.0f, 4.0f } }, glm::vec4(1.0f));
	UIDrawList moved;
	moved.AppendTransformed(plain, 1.0f, { 100.0f, 0.0f }, 1.0f, { 0.0f, 0.0f });
	UIGeometry geometry;
	moved.Build(geometry);
	REQUIRE(geometry.Vertices.size() == 4);
	CHECK(geometry.Vertices[0].Clip.x == k_UINoClip.Min.x);
	CHECK(geometry.Vertices[0].Clip.z == k_UINoClip.Max.x);

	UIDrawList twice;
	twice.Append(plain);
	twice.Append(plain);
	CHECK(twice.GetCommandCount() == 2);
	CHECK(twice.Hash() != plain.Hash());
	UIDrawList again;
	again.Append(plain);
	CHECK(again.Hash() == plain.Hash());
}
