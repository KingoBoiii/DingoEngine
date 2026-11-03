#pragma once

// Engine-internal: a model's `.ragdoll` sidecar (Model::LoadRagdollRecipes), parsed without the model,
// so the format is testable on its own.

#include "DingoEngine/Graphics/RagdollRecipe.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo::Internal
{

	struct RagdollRecipeWarning
	{
		uint32_t Line = 0;
		std::string Message;
	};

	struct RagdollRecipeFile
	{
		std::vector<RagdollRecipe> Recipes; // the default (unnamed) one first, when the file has lines for it
		std::vector<RagdollRecipeWarning> Warnings;
	};

	// `hasJoint` rejects lines naming a joint the skeleton lacks; empty accepts every name.
	RagdollRecipeFile ParseRagdollRecipes(std::string_view text, const std::function<bool(std::string_view)>& hasJoint);

}
