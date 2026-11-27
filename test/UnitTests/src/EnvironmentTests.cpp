#include <doctest/doctest.h>

#include <glm/glm.hpp>

#include "DingoEngine/Asset/AssetTypes.h"
#include "DingoEngine/Graphics/EnvironmentLayout.h"

#include <cmath>

// The engine's own Environment.glsl, compiled as C++ over GLM (see BrdfTests.cpp).
namespace GlslEnvironment
{
	using glm::vec2;
	using glm::vec3;
	using glm::asin;
	using glm::atan;
	using glm::clamp;
	using glm::cos;
	using glm::sin;

#include "DingoEngine/Graphics/Shaders/Environment.glsl"
}

namespace
{
	bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance = 1e-5f)
	{
		return glm::length(a - b) <= tolerance;
	}
}

TEST_CASE("Environment equirect: the convention puts -Z at u 0.5, +X at u 0.75 and +Y at the top")
{
	using namespace GlslEnvironment;
	CHECK(DE_EquirectUv(vec3(0, 0, -1)).x == doctest::Approx(0.5f));
	CHECK(DE_EquirectUv(vec3(0, 0, -1)).y == doctest::Approx(0.5f));
	CHECK(DE_EquirectUv(vec3(1, 0, 0)).x == doctest::Approx(0.75f));
	CHECK(DE_EquirectUv(vec3(-1, 0, 0)).x == doctest::Approx(0.25f));
	CHECK(DE_EquirectUv(vec3(0, 1, 0)).y == doctest::Approx(1.0f));
	CHECK(DE_EquirectUv(vec3(0, -1, 0)).y == doctest::Approx(0.0f));
	const float behind = DE_EquirectUv(vec3(0, 0, 1)).x;
	CHECK((behind < 1e-4f || behind > 1.0f - 1e-4f));
}

TEST_CASE("Environment equirect: direction to uv and back round-trips")
{
	using namespace GlslEnvironment;
	for (int i = 0; i < 50; ++i)
	{
		for (int j = 1; j < 25; ++j)
		{
			const vec2 uv((static_cast<float>(i) + 0.5f) / 50.0f, static_cast<float>(j) / 25.0f);
			const vec3 dir = DE_EquirectDirection(uv);
			CHECK(glm::length(dir) == doctest::Approx(1.0f));
			const vec2 back = DE_EquirectUv(dir);
			CHECK(std::abs(back.x - uv.x) < 1e-4f);
			CHECK(std::abs(back.y - uv.y) < 1e-4f);
		}
	}
	CHECK(Near(DE_EquirectDirection(vec2(0.5f, 0.5f)), vec3(0, 0, -1)));
	CHECK(Near(DE_EquirectDirection(vec2(0.75f, 0.5f)), vec3(1, 0, 0)));
	CHECK(Near(DE_EquirectDirection(vec2(0.5f, 1.0f)), vec3(0, 1, 0)));
}

TEST_CASE("Environment rotation: a positive angle turns +X toward -Z, as about +Y right-handed")
{
	using namespace GlslEnvironment;
	CHECK(Near(DE_RotateY(vec3(1, 0, 0), 0.0f, 1.0f), vec3(0, 0, -1)));
	CHECK(Near(DE_RotateY(vec3(0, 0, -1), 0.0f, 1.0f), vec3(-1, 0, 0)));
	CHECK(Near(DE_RotateY(vec3(0, 1, 0), 0.0f, 1.0f), vec3(0, 1, 0)));
	CHECK(Near(DE_RotateY(vec3(0.6f, 0.2f, -0.3f), 1.0f, 0.0f), vec3(0.6f, 0.2f, -0.3f)));

	const float angle = glm::radians(90.0f);
	const vec3 lookup = DE_RotateY(vec3(-1, 0, 0), std::cos(-angle), std::sin(-angle));
	CHECK(Near(lookup, vec3(0, 0, -1)));
}

TEST_CASE("Environment face size: auto is a quarter of the width as a power of two within 32 to 1024")
{
	using Dingo::Internal::ChooseEnvironmentFaceSize;
	CHECK(ChooseEnvironmentFaceSize(4096, 0) == 1024);
	CHECK(ChooseEnvironmentFaceSize(2048, 0) == 512);
	CHECK(ChooseEnvironmentFaceSize(1000, 0) == 128);
	CHECK(ChooseEnvironmentFaceSize(128, 0) == 32);
	CHECK(ChooseEnvironmentFaceSize(16, 0) == 32);
	CHECK(ChooseEnvironmentFaceSize(16384, 0) == 1024);
	CHECK(ChooseEnvironmentFaceSize(0, 0) == 0);
}

TEST_CASE("Environment face size: an explicit size is a power of two from 16 to 2048")
{
	using Dingo::Internal::ChooseEnvironmentFaceSize;
	CHECK(ChooseEnvironmentFaceSize(4096, 16) == 16);
	CHECK(ChooseEnvironmentFaceSize(128, 2048) == 2048);
	CHECK(ChooseEnvironmentFaceSize(4096, 256) == 256);
	CHECK(ChooseEnvironmentFaceSize(4096, 8) == 0);
	CHECK(ChooseEnvironmentFaceSize(4096, 4096) == 0);
	CHECK(ChooseEnvironmentFaceSize(4096, 100) == 0);
	CHECK(ChooseEnvironmentFaceSize(4096, 24) == 0);
}

TEST_CASE("Environment asset type: .hdr maps to Environment in any case, and it has a name")
{
	using namespace Dingo;
	CHECK(AssetTypeFromExtension(".hdr") == AssetType::Environment);
	CHECK(AssetTypeFromExtension(".HDR") == AssetType::Environment);
	CHECK(AssetTypeFromExtension(".exr") == AssetType::None);
	CHECK(std::string(ToString(AssetType::Environment)) == "Environment");
}
