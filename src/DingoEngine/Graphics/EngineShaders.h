#pragma once
#include "DingoEngine/Graphics/Shader.h"

#include <filesystem>
#include <string_view>

namespace Dingo::Internal
{

	// The engine's own shader files (src/DingoEngine/Graphics/Shaders), compiled into the library by
	// scripts/embed.lua. Debug builds read them from DE_ENGINE_SHADER_DIR while it exists, so they
	// hot-reload; every other build, and a Debug build away from its source tree, uses the copy in the
	// library.

	// The copy in the library, or empty for a name that isn't one ("Renderer3D_Lit.glsl").
	std::string_view FindEmbeddedEngineShader(std::string_view fileName);

	// The file in the source tree, or empty when this build doesn't read engine shaders from disk or
	// the file isn't there.
	std::filesystem::path FindEngineShaderFile(std::string_view fileName);

	// The shader from its source file when FindEngineShaderFile finds it (watch it with
	// WatchUnmanagedShader), otherwise from the embedded copy. params' FilePath and SourceCode are
	// replaced.
	Shader* CreateEngineShader(ShaderParams params, std::string_view fileName);

}
