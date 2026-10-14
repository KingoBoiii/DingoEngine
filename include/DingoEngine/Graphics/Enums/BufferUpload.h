#pragma once

#include <cstdint>

namespace Dingo
{

	/// When a GraphicsBuffer's data reaches the GPU (GraphicsBufferParams::Upload).
	enum class BufferUpload : uint8_t
	{
		/// Every GraphicsBuffer::Upload runs at once in a command list of its own: for data written
		/// once, which must not be lost in a frame that renders nothing.
		Direct,
		/// Upload only keeps the pointer; Renderer::Upload writes it into the frame's command list,
		/// ahead of the draws that read it.
		Deferred,
	};

}
