#pragma once

#include <cstdint>

namespace Dingo
{

	/// Width of one index in an index buffer.
	enum class IndexFormat : uint8_t
	{
		Uint16, ///< 16-bit indices.
		Uint32, ///< 32-bit indices.
	};

}
