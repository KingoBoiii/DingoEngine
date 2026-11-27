#pragma once
#include "DingoEngine/Graphics/Environment.h"

namespace Dingo::Internal
{

	struct EnvironmentAccess
	{
		// Reads a Radiance .hdr through the VFS into RGBA16F halves, top row last as the loader leaves
		// them. No graphics device needed, so a loader thread may call it. Logs and returns false on failure.
		static bool DecodeHdr(const std::filesystem::path& path, std::vector<uint16_t>& halves, uint32_t& width, uint32_t& height);

		// width x height RGBA16F texels, top row last as the loader leaves them. nullptr, with an
		// error, for a bad face size or when the textures can't be made.
		static Environment* Create(std::vector<uint16_t>&& halves, uint32_t width, uint32_t height, uint32_t faceSize, const std::string& debugName);

		// Replaces the pixels in place: the cube keeps its Texture::GetId and the environment must be
		// baked again. The previous content stays when it fails.
		static bool Reload(Environment* environment, std::vector<uint16_t>&& halves, uint32_t width, uint32_t height, uint32_t faceSize);
	};

}
