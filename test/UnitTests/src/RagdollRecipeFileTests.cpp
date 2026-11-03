#include <doctest/doctest.h>

#include "DingoEngine/Graphics/RagdollRecipeFile.h"

#include <glm/glm.hpp>

#include <string_view>

using namespace Dingo;

namespace
{

	Internal::RagdollRecipeFile Parse(std::string_view text)
	{
		return Internal::ParseRagdollRecipes(text, [](std::string_view joint) { return joint != "missing"; });
	}

}

TEST_SUITE("RagdollRecipeFile")
{
	TEST_CASE("the default recipe and a named section")
	{
		const auto file = Parse(
			"# comment\n"
			"exclude tail neck\n"
			"hip shape=box radius=0.2 mass=0.4\n"
			"[heavy]\n"
			"hip mass=0.6\n");
		CHECK(file.Warnings.empty());
		REQUIRE(file.Recipes.size() == 2);
		const RagdollRecipe& base = file.Recipes[0];
		CHECK(base.Name.empty());
		CHECK(base.IsExcluded("tail"));
		CHECK(base.IsExcluded("neck"));
		CHECK_FALSE(base.IsExcluded("hip"));
		const RagdollJointRecipe* hip = base.FindJoint("hip");
		REQUIRE(hip);
		CHECK(hip->Shape == RagdollShape::Box);
		CHECK(hip->Radius == doctest::Approx(0.2f));
		CHECK(hip->MassShare == doctest::Approx(0.4f));
		CHECK(hip->Type == RagdollJointType::Auto);
		CHECK(file.Recipes[1].Name == "heavy");
		REQUIRE(file.Recipes[1].FindJoint("hip"));
		CHECK(file.Recipes[1].FindJoint("hip")->MassShare == doctest::Approx(0.6f));
		CHECK_FALSE(file.Recipes[1].FindJoint("knee"));
	}

	TEST_CASE("a file with only sections has no default recipe")
	{
		const auto file = Parse("[a]\nhip mass=0.5\n[b]\n");
		REQUIRE(file.Recipes.size() == 2);
		CHECK(file.Recipes[0].Name == "a");
		CHECK(file.Recipes[1].Name == "b");
	}

	TEST_CASE("hinge, swing and twist limits")
	{
		const auto file = Parse(
			"knee hinge=-5..150\n"
			"shoulder swing=60,40 twist=-30..20\n"
			"neck swing=45 twist=25\n");
		CHECK(file.Warnings.empty());
		REQUIRE(file.Recipes.size() == 1);
		const RagdollRecipe& recipe = file.Recipes[0];
		const RagdollJointRecipe* knee = recipe.FindJoint("knee");
		REQUIRE(knee);
		CHECK(knee->Type == RagdollJointType::Hinge);
		CHECK(knee->HasHingeLimits);
		CHECK(knee->HingeMin == doctest::Approx(glm::radians(-5.0f)));
		CHECK(knee->HingeMax == doctest::Approx(glm::radians(150.0f)));
		const RagdollJointRecipe* shoulder = recipe.FindJoint("shoulder");
		REQUIRE(shoulder);
		CHECK(shoulder->Type == RagdollJointType::SwingTwist);
		CHECK(shoulder->SwingY == doctest::Approx(glm::radians(60.0f)));
		CHECK(shoulder->SwingZ == doctest::Approx(glm::radians(40.0f)));
		CHECK(shoulder->TwistMin == doctest::Approx(glm::radians(-30.0f)));
		CHECK(shoulder->TwistMax == doctest::Approx(glm::radians(20.0f)));
		const RagdollJointRecipe* neck = recipe.FindJoint("neck");
		REQUIRE(neck);
		CHECK(neck->SwingY == neck->SwingZ);
		CHECK(neck->TwistMin == doctest::Approx(glm::radians(-25.0f)));
		CHECK(neck->TwistMax == doctest::Approx(glm::radians(25.0f)));
	}

	TEST_CASE("a bad value drops only its key")
	{
		const auto file = Parse("hip shape=cone radius=-1 mass=2 hinge=10..5 swing=200 color=red radius=0.3\n");
		CHECK(file.Warnings.size() == 6);
		CHECK(file.Warnings[0].Line == 1);
		REQUIRE(file.Recipes.size() == 1);
		const RagdollJointRecipe* hip = file.Recipes[0].FindJoint("hip");
		REQUIRE(hip);
		CHECK(hip->Shape == RagdollShape::Capsule);
		CHECK(hip->Radius == doctest::Approx(0.3f));
		CHECK(hip->MassShare == 0.0f);
		CHECK(hip->Type == RagdollJointType::Auto);
	}

	TEST_CASE("a twist range has to hold the build pose")
	{
		const auto file = Parse("hip twist=10..40\n");
		CHECK(file.Warnings.size() == 1);
		REQUIRE(file.Recipes.size() == 1);
		REQUIRE(file.Recipes[0].FindJoint("hip"));
		CHECK_FALSE(file.Recipes[0].FindJoint("hip")->HasTwist);
	}

	TEST_CASE("an unknown joint skips its line")
	{
		const auto file = Parse("missing mass=0.5\nexclude missing tail\n");
		CHECK(file.Warnings.size() == 2);
		REQUIRE(file.Recipes.size() == 1);
		CHECK(file.Recipes[0].Joints.empty());
		CHECK(file.Recipes[0].Excluded.size() == 1);
		CHECK(file.Recipes[0].IsExcluded("tail"));
	}

	TEST_CASE("a hinge with swing limits skips the line")
	{
		const auto file = Parse("knee hinge=0..90 swing=30\n");
		REQUIRE(file.Warnings.size() == 1);
		CHECK(file.Recipes.empty());
	}

	TEST_CASE("a second line for a joint wins, a repeated section joins the first")
	{
		const auto file = Parse("[a]\nhip mass=0.2\n[b]\n[a]\nhip mass=0.3\n");
		CHECK(file.Warnings.size() == 2);
		REQUIRE(file.Recipes.size() == 2);
		REQUIRE(file.Recipes[0].Joints.size() == 1);
		CHECK(file.Recipes[0].Joints[0].MassShare == doctest::Approx(0.3f));
	}

	TEST_CASE("a BOM, CRLF and a malformed section")
	{
		const auto file = Parse("\xEF\xBB\xBFhip mass=0.5\r\n[]\r\n[open\r\n");
		CHECK(file.Warnings.size() == 2);
		REQUIRE(file.Recipes.size() == 1);
		REQUIRE(file.Recipes[0].FindJoint("hip"));
		CHECK(file.Recipes[0].FindJoint("hip")->MassShare == doctest::Approx(0.5f));
	}
}
