#pragma once

namespace Dingo
{

	/// What a GraphicsBuffer is bound as (GraphicsBufferParams::Type).
	enum class BufferType
	{
		Unknown, ///< Not set; give a buffer a real type.

		VertexBuffer,
		IndexBuffer,
		/// A uniform block; the engine's are volatile and must be written every frame they are
		/// bound.
		UniformBuffer,
		StorageBuffer, ///< A shader storage block (std430); compute can write it, never volatile.

		Count ///< The number of types, not a type.
	};

}
