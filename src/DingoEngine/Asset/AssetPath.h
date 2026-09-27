#pragma once

#include <filesystem>

namespace Dingo::Internal
{

	// Resolves a path handed to a raw file factory the way AssetManager does: a relative path
	// that names a file under the asset root resolves there. Anything else — absolute, empty,
	// not under the root, or no manager initialised — is returned unchanged, so the working
	// directory stays a fallback for code written before the root existed.
	std::filesystem::path ResolveRawAssetPath(const std::filesystem::path& path);

}
