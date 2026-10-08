#pragma once

namespace Dingo
{

	// When a fragment passes the depth test, comparing its depth against the stored one.
	enum class DepthCompare
	{
		Less,
		LessEqual,
		Greater,
		GreaterEqual,
		Equal,
		Always
	};

}
