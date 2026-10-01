#pragma once

namespace Dingo
{
	class Shader;
}

namespace Dingo::Internal
{

	// Hot-reloads a file-backed shader no AssetManager owns (the engine's own built-in shaders) on
	// the manager's poll, whenever hot-reload is enabled. The list outlives any one manager, so a
	// renderer can register before the AssetManager exists; it must unwatch before destroying the
	// shader. Inline-source shaders are ignored.
	void WatchUnmanagedShader(Shader* shader);
	void UnwatchUnmanagedShader(Shader* shader);

}
