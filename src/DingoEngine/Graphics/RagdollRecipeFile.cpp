#include "depch.h"
#include "DingoEngine/Graphics/RagdollRecipeFile.h"
#include "DingoEngine/Graphics/EventsFile.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <format>
#include <sstream>

namespace Dingo
{

	const RagdollJointRecipe* RagdollRecipe::FindJoint(std::string_view joint) const
	{
		const auto it = std::find_if(Joints.begin(), Joints.end(), [&](const RagdollJointRecipe& entry) { return entry.Joint == joint; });
		return it != Joints.end() ? &*it : nullptr;
	}

	bool RagdollRecipe::IsExcluded(std::string_view joint) const
	{
		return std::find(Excluded.begin(), Excluded.end(), joint) != Excluded.end();
	}

	namespace
	{

		bool ParseFloat(std::string_view text, float& value)
		{
			if (!text.empty() && text.front() == '+')
				text.remove_prefix(1);
			const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
			return !text.empty() && error == std::errc() && end == text.data() + text.size() && std::isfinite(value);
		}

		// `<a>..<b>` into a and b, or a lone `<a>` into -|a|..|a| when `symmetric`.
		bool ParseRange(std::string_view text, bool symmetric, float& low, float& high)
		{
			const size_t dots = text.find("..");
			if (dots == std::string_view::npos)
			{
				float value = 0.0f;
				if (!symmetric || !ParseFloat(text, value))
					return false;
				low = -std::abs(value);
				high = std::abs(value);
				return true;
			}
			return ParseFloat(text.substr(0, dots), low) && ParseFloat(text.substr(dots + 2), high);
		}

		// `<y>,<z>`, or a lone `<a>` for both.
		bool ParsePair(std::string_view text, float& first, float& second)
		{
			const size_t comma = text.find(',');
			if (comma == std::string_view::npos)
			{
				if (!ParseFloat(text, first))
					return false;
				second = first;
				return true;
			}
			return ParseFloat(text.substr(0, comma), first) && ParseFloat(text.substr(comma + 1), second);
		}

		std::string_view Trim(std::string_view text)
		{
			while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
				text.remove_prefix(1);
			while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
				text.remove_suffix(1);
			return text;
		}

	}

	Internal::RagdollRecipeFile Internal::ParseRagdollRecipes(std::string_view text, const std::function<bool(std::string_view)>& hasJoint)
	{
		RagdollRecipeFile file;
		RagdollRecipe* current = nullptr;
		auto recipe = [&]() -> RagdollRecipe&
		{
			if (!current)
			{
				file.Recipes.emplace_back();
				current = &file.Recipes.back();
			}
			return *current;
		};
		auto warn = [&](uint32_t line, std::string message) { file.Warnings.push_back({ line, std::move(message) }); };
		auto known = [&](std::string_view joint) { return !hasJoint || hasJoint(joint); };

		std::istringstream stream{ std::string(text) };
		std::string line;
		std::vector<std::string> tokens;
		for (uint32_t number = 1; std::getline(stream, line); ++number)
		{
			if (number == 1 && line.starts_with("\xEF\xBB\xBF"))
				line.erase(0, 3);
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			const std::string_view trimmed = Trim(line);
			if (trimmed.starts_with('['))
			{
				const size_t close = trimmed.find(']');
				const std::string_view rest = close == std::string_view::npos ? std::string_view() : Trim(trimmed.substr(close + 1));
				const std::string_view name = close == std::string_view::npos ? std::string_view() : Trim(trimmed.substr(1, close - 1));
				if (close == std::string_view::npos || name.empty() || (!rest.empty() && rest.front() != '#'))
				{
					warn(number, "expected `[recipe name]`; line skipped");
					continue;
				}

				const auto existing = std::find_if(file.Recipes.begin(), file.Recipes.end(), [&](const RagdollRecipe& r) { return r.Name == name; });
				if (existing != file.Recipes.end())
				{
					warn(number, std::format("recipe '{}' appears twice; its lines join the first", name));
					current = &*existing;
				}
				else
				{
					// Pointers into Recipes move when it grows, so `current` is taken after the push.
					file.Recipes.emplace_back().Name = std::string(name);
					current = &file.Recipes.back();
				}
				continue;
			}

			if (!TokenizeSidecarLine(line, tokens))
			{
				warn(number, "an unclosed quote; line skipped");
				continue;
			}
			if (tokens.empty())
				continue;

			if (tokens[0] == "exclude")
			{
				if (tokens.size() < 2)
				{
					warn(number, "expected `exclude <joint> [<joint>...]`; line skipped");
					continue;
				}
				RagdollRecipe& target = recipe();
				for (size_t i = 1; i < tokens.size(); ++i)
				{
					if (!known(tokens[i]))
						warn(number, std::format("the skeleton has no joint '{}'; it is ignored", tokens[i]));
					else if (!target.IsExcluded(tokens[i]))
						target.Excluded.push_back(tokens[i]);
				}
				continue;
			}

			if (tokens.size() < 2)
			{
				warn(number, "expected `<joint> key=value...` or `exclude <joint>`; line skipped");
				continue;
			}
			if (!known(tokens[0]))
			{
				warn(number, std::format("the skeleton has no joint '{}'; line skipped", tokens[0]));
				continue;
			}

			RagdollJointRecipe entry;
			entry.Joint = tokens[0];
			for (size_t i = 1; i < tokens.size(); ++i)
			{
				const std::string_view token = tokens[i];
				const size_t equals = token.find('=');
				const std::string_view key = token.substr(0, equals);
				const std::string_view value = equals == std::string_view::npos ? std::string_view() : token.substr(equals + 1);
				auto bad = [&](std::string_view expected) { warn(number, std::format("'{}' isn't {}; it is ignored", token, expected)); };

				if (key == "shape")
				{
					if (value == "capsule")
						entry.Shape = RagdollShape::Capsule;
					else if (value == "sphere")
						entry.Shape = RagdollShape::Sphere;
					else if (value == "box")
						entry.Shape = RagdollShape::Box;
					else
						bad("shape=capsule, sphere or box");
				}
				else if (key == "radius")
				{
					float radius = 0.0f;
					if (ParseFloat(value, radius) && radius > 0.0f)
						entry.Radius = radius;
					else
						bad("radius=<a length above 0>");
				}
				else if (key == "mass")
				{
					float share = 0.0f;
					if (ParseFloat(value, share) && share > 0.0f && share <= 1.0f)
						entry.MassShare = share;
					else
						bad("mass=<a share of the total above 0, at most 1>");
				}
				else if (key == "hinge")
				{
					float low = 0.0f, high = 0.0f;
					if (ParseRange(value, false, low, high) && low < high && low >= -180.0f && high <= 180.0f)
					{
						entry.Type = RagdollJointType::Hinge;
						entry.HingeMin = glm::radians(low);
						entry.HingeMax = glm::radians(high);
						entry.HasHingeLimits = true;
					}
					else
						bad("hinge=<min>..<max> in degrees, min below max, within -180..180");
				}
				else if (key == "swing")
				{
					float y = 0.0f, z = 0.0f;
					if (ParsePair(value, y, z) && y > 0.0f && z > 0.0f && y < 180.0f && z < 180.0f)
					{
						entry.Type = RagdollJointType::SwingTwist;
						entry.SwingY = glm::radians(y);
						entry.SwingZ = glm::radians(z);
						entry.HasSwing = true;
					}
					else
						bad("swing=<degrees> or swing=<y>,<z>, each between 0 and 180");
				}
				else if (key == "twist")
				{
					float low = 0.0f, high = 0.0f;
					if (ParseRange(value, true, low, high) && low <= 0.0f && high >= 0.0f && low < high && low >= -180.0f && high <= 180.0f)
					{
						entry.Type = RagdollJointType::SwingTwist;
						entry.TwistMin = glm::radians(low);
						entry.TwistMax = glm::radians(high);
						entry.HasTwist = true;
					}
					else
						bad("twist=<degrees> or twist=<min>..<max>, min at most 0 and max at least 0 (the build pose), within -180..180");
				}
				else
				{
					warn(number, std::format("unknown key '{}' (shape, radius, mass, hinge, swing, twist); it is ignored", key));
				}
			}

			if (entry.HasHingeLimits && (entry.HasSwing || entry.HasTwist))
			{
				warn(number, std::format("'{}' asks for both a hinge and swing/twist limits; line skipped", entry.Joint));
				continue;
			}

			RagdollRecipe& target = recipe();
			if (auto it = std::find_if(target.Joints.begin(), target.Joints.end(), [&](const RagdollJointRecipe& j) { return j.Joint == entry.Joint; }); it != target.Joints.end())
			{
				warn(number, std::format("'{}' already has a line in this recipe; the later one wins", entry.Joint));
				*it = std::move(entry);
			}
			else
			{
				target.Joints.push_back(std::move(entry));
			}
		}
		return file;
	}

}
