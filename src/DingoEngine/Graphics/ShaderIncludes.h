#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Dingo::Internal
{

	// Replaces every #include line in a shader's source with the file it names, recursively, before
	// the source is hashed for the bytecode cache and compiled, so an edit to an included file is a new
	// hash. A file is pasted once; a second #include of it adds nothing.
	//
	//   #include "Lighting.glsl"            next to the file holding the line: sourcePath's directory
	//                                       for the shader's own source (the asset root, then the working
	//                                       directory, for an inline shader), an engine shader's
	//                                       neighbours for a line inside one
	//   #include <DingoEngine/Shadows.glsl> an engine shader (EngineShaders.h)
	//
	// includedFiles receives every file read from disk, for the hot-reload watch. Returns nullopt, with
	// the reason logged, when a file can't be found or includes nest past 32 levels.
	std::optional<std::string> ExpandShaderIncludes(const std::string& source, const std::filesystem::path& sourcePath, const std::string& shaderName, std::vector<std::filesystem::path>& includedFiles);

}
