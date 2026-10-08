#pragma once

#include <cstdint>

namespace Dingo
{
	class Framebuffer;
}

namespace Dingo::Internal
{

	// A hash of what a pipeline is built against: the colour formats, depth format and sample
	// count, not the size. A pipeline built for one framebuffer draws into any with the same key.
	// 0 for a null framebuffer.
	uint64_t GetFramebufferFormatKey(const Framebuffer* framebuffer);

}
