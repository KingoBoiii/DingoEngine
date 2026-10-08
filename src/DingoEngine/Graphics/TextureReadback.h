#pragma once

#include "DingoEngine/Graphics/Texture.h"

#include <functional>
#include <memory>

namespace Dingo::Internal
{

	// Reads a texture back without waiting on the GPU: Read records the copy into the frame being
	// recorded, a GPU event is set behind it at the next frame's start, and the pixels are mapped and
	// handed to done at the first frame start that finds the event signalled (one to three frames on),
	// or at Renderer::Shutdown. Texture::ReadPixels maps at the next frame start, which waits for the
	// GPU to finish that frame. One read at a time; the staging memory is kept for the next.
	class TextureReadback
	{
	public:
		static std::shared_ptr<TextureReadback> Create();
		virtual ~TextureReadback() = default;

		virtual bool IsBusy() const = 0;
		// False, with nothing queued, while busy, outside a recording frame, or for a texture
		// Texture::ReadPixels can't read.
		virtual bool Read(Texture* source, std::function<void(const TexturePixels&)> done) = 0;
	};

}
